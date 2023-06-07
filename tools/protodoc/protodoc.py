# protoc plugin to map from FileDescriptorProtos to Envoy doc style RST.
# See https://github.com/google/protobuf/blob/master/src/google/protobuf/descriptor.proto
# for the underlying protos mentioned in this file. See
# https://www.sphinx-doc.org/en/master/usage/restructuredtext/basics.html for Sphinx RST syntax.

import logging
import functools
import sys
from collections import defaultdict
from functools import cached_property

import yaml

from envoy.code.check.checker import BackticksCheck

from tools.api_proto_plugin import annotations, constants
from tools.api_proto_plugin import plugin
from tools.api_proto_plugin import visitor
from tools.protodoc.data import data
from tools.protodoc.jinja import env as jinja_env

from udpa.annotations import security_pb2
from udpa.annotations import status_pb2 as udpa_status_pb2
from validate import validate_pb2
from xds.annotations.v3 import status_pb2 as xds_status_pb2

logger = logging.getLogger(__name__)

manifest_db = data["manifest"]
EXTENSION_DB = data["extensions"]
CONTRIB_EXTENSION_DB = data["contrib_extensions"]
EXTENSION_CATEGORIES = data["extension_categories"]
CONTRIB_EXTENSION_CATEGORIES = data["contrib_extension_categories"]
EXTENSION_SECURITY_POSTURES = data["extension_security_postures"]
EXTENSION_STATUS_VALUES = data["extension_status_values"]

WIP_WARNING = (
    '.. warning::\n   This API feature is currently work-in-progress. API features marked as '
    'work-in-progress are not considered stable, are not covered by the :ref:`threat model '
    '<arch_overview_threat_model>`, are not supported by the security team, and are subject to '
    'breaking changes. Do not use this feature without understanding each of the previous '
    'points.\n\n')


class ProtodocError(Exception):
    """Base error class for the protodoc module."""


def hide_not_implemented(comment):
    """Should a given type_context.Comment be hidden because it is tagged as [#not-implemented-hide:]?"""
    return annotations.NOT_IMPLEMENTED_HIDE_ANNOTATION in comment.annotations


def github_url(text, type_context):
    """Obtain data plane API Github URL by path from a TypeContext.

    Args:
        type_context: type_context.TypeContext for node.

    Returns:
        A string with a corresponding data plane API GitHub Url.
    """
    if type_context.name.startswith(constants.CNCF_PREFIX[1:]):
        return format_external_link(
            text,
            f"https://github.com/cncf/xds/blob/main/{type_context.source_code_info.name}#L{type_context.location.span[0]}"
        )
    return f":repo:`{text} <api/{type_context.source_code_info.name}#L{type_context.location.span[0]}>`"


def format_comment_with_annotations(comment, show_wip_warning=False):
    """Format a comment string with additional RST for annotations.

    Args:
        comment: comment string.
        show_wip_warning: whether to show the work in progress warning.

    Returns:
        A string with additional RST from annotations.
    """
    wip_warning = ''
    if show_wip_warning:
        wip_warning = WIP_WARNING

    formatted_extension = ''
    if annotations.EXTENSION_ANNOTATION in comment.annotations:
        extension = comment.annotations[annotations.EXTENSION_ANNOTATION]
        formatted_extension = format_extension(extension)
    formatted_extension_category = ''
    if annotations.EXTENSION_CATEGORY_ANNOTATION in comment.annotations:
        for category in comment.annotations[annotations.EXTENSION_CATEGORY_ANNOTATION].split(","):
            formatted_extension_category += format_extension_category(category)
    comment = annotations.without_annotations(comment.raw + '\n')
    return comment + wip_warning + formatted_extension + formatted_extension_category


def map_lines(f, s):
    """Apply a function across each line in a flat string.

    Args:
        f: A string transform function for a line.
        s: A string consisting of potentially multiple lines.

    Returns:
        A flat string with f applied to each line.
    """
    return '\n'.join(f(line) for line in s.split('\n'))


def indent(spaces, line):
    """Indent a string."""
    return ' ' * spaces + line


def indent_lines(spaces, lines):
    """Indent a list of strings."""
    return map(functools.partial(indent, spaces), lines)


def format_internal_link(text, ref):
    return ':ref:`%s <%s>`' % (text, ref)


def format_external_link(text, ref):
    return '`%s <%s>`_' % (text, ref)


def format_header(style, text, is_file=False):
    """Format RST header.

    Args:
        style: underline style, e.g. '=', '-'.
        text: header text

    Returns:
        RST formatted header.
    """
    if is_file:
        text = f"{text} (proto)"
    return '%s\n%s\n\n' % (text, style * len(text))


def format_extension(extension):
    """Format extension metadata as RST.

    Args:
        extension: the name of the extension, e.g. com.acme.foo.

    Returns:
        RST formatted extension description.
    """
    try:
        extension_metadata = EXTENSION_DB.get(extension, None)
        contrib = ''
        if extension_metadata is None:
            extension_metadata = CONTRIB_EXTENSION_DB[extension]
            contrib = """

.. note::
  This extension is only available in :ref:`contrib <install_contrib>` images.

"""
        status = (EXTENSION_STATUS_VALUES.get(extension_metadata.get('status')) or "").strip()
        security_posture = EXTENSION_SECURITY_POSTURES[
            extension_metadata['security_posture']].strip()
        categories = extension_metadata["categories"]
        type_urls = extension_metadata.get('type_urls') or []
    except KeyError as e:
        sys.stderr.write(
            f"\n\nDid you forget to add '{extension}' to extensions_build_config.bzl, "
            "extensions_metadata.yaml, contrib_build_config.bzl, "
            "or contrib/extensions_metadata.yaml?\n\n")
        exit(1)  # Raising the error buries the above message in tracebacks.

    extension = jinja_env.get_template("extension.rst.tpl").render(
        extension=extension,
        contrib=contrib,
        status=status,
        security_posture=security_posture,
        categories=categories,
        type_urls=type_urls)
    return f"\n{extension}\n"


def format_extension_category(extension_category):
    """Format extension metadata as RST.

    Args:
        extension_category: the name of the extension_category, e.g. com.acme.

    Returns:
        RST formatted extension category description.
    """
    extensions = EXTENSION_CATEGORIES.get(extension_category, [])
    contrib_extensions = CONTRIB_EXTENSION_CATEGORIES.get(extension_category, [])
    if not extensions and not contrib_extensions:
        raise ProtodocError(f"\n\nUnable to find extension category:  {extension_category}\n\n")
    extension_category = jinja_env.get_template("extension_category.rst.tpl").render(
        category=extension_category,
        extensions=sorted(extensions),
        contrib_extensions=sorted(contrib_extensions))
    return f"\n{extension_category}\n"


def format_header_from_file(style, source_code_info, proto_name):
    """Format RST header based on special file level title

    Args:
        style: underline style, e.g. '=', '-'.
        source_code_info: SourceCodeInfo object.
        proto_name: If the file_level_comment does not contain a user specified
           title, use this as page title.

    Returns:
        RST formatted header, and file level comment without page title strings.
    """
    anchor = format_anchor(file_cross_ref_label(proto_name))
    stripped_comment = annotations.without_annotations(
        '\n'.join(c + '\n' for c in source_code_info.file_level_comments))
    formatted_extension = ''
    if annotations.EXTENSION_ANNOTATION in source_code_info.file_level_annotations:
        extension = source_code_info.file_level_annotations[annotations.EXTENSION_ANNOTATION]
        formatted_extension = format_extension(extension)
    if annotations.DOC_TITLE_ANNOTATION in source_code_info.file_level_annotations:
        return anchor + format_header(
            style, source_code_info.file_level_annotations[annotations.DOC_TITLE_ANNOTATION],
            True) + "\n\n" + formatted_extension, stripped_comment
    return anchor + format_header(
        style, proto_name, True) + "\n\n" + formatted_extension, stripped_comment


def format_field_type_as_json(type_context, field):
    """Format FieldDescriptorProto.Type as a pseudo-JSON string.

    Args:
        type_context: contextual information for message/enum/field.
        field: FieldDescriptor proto.
    Return: RST formatted pseudo-JSON string representation of field type.
    """
    if type_name_from_fqn(field.type_name) in type_context.map_typenames:
        return "{...}"
    if field.label == field.LABEL_REPEATED:
        return "[]"
    if field.type == field.TYPE_MESSAGE:
        return "{...}"
    return "..."


def format_message_as_json(type_context, msg):
    """Format a message definition DescriptorProto as a pseudo-JSON block.

    Args:
        type_context: contextual information for message/enum/field.
        msg: message definition DescriptorProto.
    Return: RST formatted pseudo-JSON string representation of message definition.
    """
    lines = []
    for index, field in enumerate(msg.field):
        field_type_context = type_context.extend_field(index, field.name)
        leading_comment = field_type_context.leading_comment
        if hide_not_implemented(leading_comment):
            continue
        lines.append('"%s": %s' % (field.name, format_field_type_as_json(type_context, field)))

    if lines:
        return '.. code-block:: json\n  :force:\n\n  {\n' + ',\n'.join(
            indent_lines(4, lines)) + '\n  }\n\n'
    return ""


def normalize_field_type_name(field_fqn):
    """Normalize a fully qualified field type name, e.g.

    .envoy.foo.bar.

    Strips leading constants.ENVOY_API_NAMESPACE_PREFIX and constants.ENVOY_PREFIX.

    Args:
        field_fqn: a fully qualified type name from FieldDescriptorProto.type_name.
    Return: Normalized type name.
    """
    if field_fqn.startswith(constants.ENVOY_API_NAMESPACE_PREFIX):
        return field_fqn[len(constants.ENVOY_API_NAMESPACE_PREFIX):]
    if field_fqn.startswith(constants.ENVOY_PREFIX):
        return field_fqn[len(constants.ENVOY_PREFIX):]
    return field_fqn


def normalize_type_context_name(type_name):
    """Normalize a type name, e.g.

    envoy.foo.bar.

    Strips leading constants.ENVOY_API_NAMESPACE_PREFIX and constants.ENVOY_PREFIX.

    Args:
        type_name: a name from a TypeContext.
    Return: Normalized type name.
    """
    return normalize_field_type_name(qualify_type_name(type_name))


def qualify_type_name(type_name):
    return '.' + type_name


def type_name_from_fqn(fqn):
    return fqn[1:]


def format_field_type(type_context, field):
    """Format a FieldDescriptorProto type description.

    Adds cross-refs for message types.
    TODO(htuch): Add cross-refs for enums as well.

    Args:
        type_context: contextual information for message/enum/field.
        field: FieldDescriptor proto.
    Return: RST formatted field type.
    """
    envoy_proto = (
        field.type_name.startswith(constants.ENVOY_API_NAMESPACE_PREFIX)
        or field.type_name.startswith(constants.ENVOY_PREFIX)
        or field.type_name.startswith(constants.CNCF_PREFIX))
    if envoy_proto:
        type_name = normalize_field_type_name(field.type_name)
        if field.type == field.TYPE_MESSAGE:
            if type_context.map_typenames and type_name_from_fqn(
                    field.type_name) in type_context.map_typenames:
                return 'map<%s, %s>' % tuple(
                    map(
                        functools.partial(format_field_type, type_context),
                        type_context.map_typenames[type_name_from_fqn(field.type_name)]))
            return format_internal_link(type_name, message_cross_ref_label(type_name))
        if field.type == field.TYPE_ENUM:
            return format_internal_link(type_name, enum_cross_ref_label(type_name))
    elif field.type_name.startswith(constants.WKT_NAMESPACE_PREFIX):
        wkt = field.type_name[len(constants.WKT_NAMESPACE_PREFIX):]
        return format_external_link(
            wkt, 'https://developers.google.com/protocol-buffers/docs/reference/google.protobuf#%s'
            % wkt.lower())
    elif field.type_name.startswith(constants.RPC_NAMESPACE_PREFIX):
        rpc = field.type_name[len(constants.RPC_NAMESPACE_PREFIX):]
        return format_external_link(
            rpc, 'https://cloud.google.com/natural-language/docs/reference/rpc/google.rpc#%s'
            % rpc.lower())
    elif field.type_name:
        return field.type_name

    if field.type in constants.FIELD_TYPE_NAMES:
        return format_external_link(
            constants.FIELD_TYPE_NAMES[field.type],
            'https://developers.google.com/protocol-buffers/docs/proto#scalar')
    raise ProtodocError('Unknown field type ' + str(field.type))


def file_cross_ref_label(msg_name):
    """File cross reference label."""
    return 'envoy_v3_api_file_%s' % msg_name


def message_cross_ref_label(msg_name):
    """Message cross reference label."""
    return 'envoy_v3_api_msg_%s' % msg_name


def enum_cross_ref_label(enum_name):
    """Enum cross reference label."""
    return 'envoy_v3_api_enum_%s' % enum_name


def field_cross_ref_label(field_name):
    """Field cross reference label."""
    return 'envoy_v3_api_field_%s' % field_name


def enum_value_cross_ref_label(enum_value_name):
    """Enum value cross reference label."""
    return 'envoy_v3_api_enum_value_%s' % enum_value_name


def format_anchor(label):
    """Format a label as an Envoy API RST anchor."""
    return '.. _%s:\n\n' % label


def format_security_options(security_option, field, type_context, edge_config):
    sections = []

    if security_option.configure_for_untrusted_downstream:
        sections.append(
            indent(
                4, 'This field should be configured in the presence of untrusted *downstreams*.'))
    if security_option.configure_for_untrusted_upstream:
        sections.append(
            indent(4, 'This field should be configured in the presence of untrusted *upstreams*.'))
    if edge_config["note"]:
        sections.append(indent(4, edge_config["note"].strip()))

    example_dict = edge_config["example"]
    field_name = type_context.name.split('.')[-1]
    example = {field_name: example_dict}
    sections.append(
        indent(4, 'Example configuration for untrusted environments:\n\n')
        + indent(4, '.. code-block:: yaml\n\n')
        + '\n'.join(indent_lines(6,
                                 yaml.dump(example).split('\n'))))

    return '.. attention::\n' + '\n\n'.join(sections)


def format_field_as_definition_list_item(
        outer_type_context, type_context, field, protodoc_manifest):
    """Format a FieldDescriptorProto as RST definition list item.

    Args:
        outer_type_context: contextual information for enclosing message.
        type_context: contextual information for message/enum/field.
        field: FieldDescriptorProto.
        protodoc_manifest: tools.protodoc.Manifest for proto.

    Returns:
        RST formatted definition list item.
    """
    field_annotations = []

    anchor = format_anchor(field_cross_ref_label(normalize_type_context_name(type_context.name)))
    if field.options.HasExtension(validate_pb2.rules):
        rule = field.options.Extensions[validate_pb2.rules]
        if ((rule.HasField('message') and rule.message.required)
                or (rule.HasField('duration') and rule.duration.required)
                or (rule.HasField('string') and rule.string.min_len > 0)
                or (rule.HasField('string') and rule.string.min_bytes > 0)
                or (rule.HasField('repeated') and rule.repeated.min_items > 0)):
            field_annotations = ['*REQUIRED*']
    leading_comment = type_context.leading_comment
    formatted_leading_comment = format_comment_with_annotations(
        leading_comment,
        field.options.HasExtension(xds_status_pb2.field_status)
        and field.options.Extensions[xds_status_pb2.field_status].work_in_progress)
    if hide_not_implemented(leading_comment):
        return ''

    if field.HasField('oneof_index'):
        oneof_context = outer_type_context.extend_oneof(
            field.oneof_index, type_context.oneof_names[field.oneof_index])
        oneof_comment = oneof_context.leading_comment
        formatted_oneof_comment = format_comment_with_annotations(oneof_comment)
        if hide_not_implemented(oneof_comment):
            return ''

        # If the oneof only has one field and marked required, mark the field as required.
        if len(type_context.oneof_fields[field.oneof_index]) == 1 and type_context.oneof_required[
                field.oneof_index]:
            field_annotations = ['*REQUIRED*']

        if len(type_context.oneof_fields[field.oneof_index]) > 1:
            # Fields in oneof shouldn't be marked as required when we have oneof comment below it.
            field_annotations = []
            oneof_template = '\nPrecisely one of %s must be set.\n' if type_context.oneof_required[
                field.oneof_index] else '\nOnly one of %s may be set.\n'
            formatted_oneof_comment += oneof_template % ', '.join(
                format_internal_link(
                    f,
                    field_cross_ref_label(
                        normalize_type_context_name(outer_type_context.extend_field(i, f).name)))
                for i, f in type_context.oneof_fields[field.oneof_index])
    else:
        formatted_oneof_comment = ''

    # If there is a udpa.annotations.security option, include it after the comment.
    if field.options.HasExtension(security_pb2.security):
        manifest_description = protodoc_manifest.get(type_context.name)
        if not manifest_description:
            raise ProtodocError('Missing protodoc manifest YAML for %s' % type_context.name)
        formatted_security_options = format_security_options(
            field.options.Extensions[security_pb2.security], field, type_context,
            manifest_description)
    else:
        formatted_security_options = ''
    comment = '(%s) ' % ', '.join(
        [constants.FIELD_LABEL_NAMES[field.label] + format_field_type(type_context, field)]
        + field_annotations) + formatted_leading_comment
    return anchor + field.name + '\n' + map_lines(
        functools.partial(indent, 2),
        comment + formatted_oneof_comment) + formatted_security_options


def format_message_as_definition_list(type_context, msg, protodoc_manifest):
    """Format a DescriptorProto as RST definition list.

    Args:
        type_context: contextual information for message/enum/field.
        msg: DescriptorProto.
        protodoc_manifest: tools.protodoc.Manifest for proto.

    Returns:
        RST formatted definition list item.
    """
    type_context.oneof_fields = defaultdict(list)
    type_context.oneof_required = defaultdict(bool)
    type_context.oneof_names = defaultdict(list)
    for index, field in enumerate(msg.field):
        if field.HasField('oneof_index'):
            leading_comment = type_context.extend_field(index, field.name).leading_comment
            if hide_not_implemented(leading_comment):
                continue
            type_context.oneof_fields[field.oneof_index].append((index, field.name))
    for index, oneof_decl in enumerate(msg.oneof_decl):
        if oneof_decl.options.HasExtension(validate_pb2.required):
            type_context.oneof_required[index] = oneof_decl.options.Extensions[
                validate_pb2.required]
        type_context.oneof_names[index] = oneof_decl.name
    return '\n'.join(
        format_field_as_definition_list_item(
            type_context, type_context.extend_field(index, field.name), field, protodoc_manifest)
        for index, field in enumerate(msg.field)) + '\n'


def format_enum_value_as_definition_list_item(type_context, enum_value):
    """Format a EnumValueDescriptorProto as RST definition list item.

    Args:
        type_context: contextual information for message/enum/field.
        enum_value: EnumValueDescriptorProto.

    Returns:
        RST formatted definition list item.
    """
    anchor = format_anchor(
        enum_value_cross_ref_label(normalize_type_context_name(type_context.name)))
    default_comment = '*(DEFAULT)* ' if enum_value.number == 0 else ''
    leading_comment = type_context.leading_comment
    formatted_leading_comment = format_comment_with_annotations(leading_comment)
    if hide_not_implemented(leading_comment):
        return ''
    comment = default_comment + constants.UNICODE_INVISIBLE_SEPARATOR + formatted_leading_comment
    return anchor + enum_value.name + '\n' + map_lines(functools.partial(indent, 2), comment)


def format_enum_as_definition_list(type_context, enum):
    """Format a EnumDescriptorProto as RST definition list.

    Args:
        type_context: contextual information for message/enum/field.
        enum: DescriptorProto.

    Returns:
        RST formatted definition list item.
    """
    return '\n'.join(
        format_enum_value_as_definition_list_item(
            type_context.extend_enum_value(index, enum_value.name), enum_value)
        for index, enum_value in enumerate(enum.value)) + '\n'


def format_proto_as_block_comment(proto):
    """Format a proto as a RST block comment.

    Useful in debugging, not usually referenced.
    """
    return '\n\nproto::\n\n' + map_lines(functools.partial(indent, 2), str(proto)) + '\n'


class RstFormatVisitor(visitor.Visitor):
    """Visitor to generate a RST representation from a FileDescriptor proto.

    See visitor.Visitor for visitor method docs comments.
    """

    def __init__(self):
        self.protodoc_manifest = manifest_db

    @cached_property
    def backticks_check(self):
        return BackticksCheck()

    def visit_enum(self, enum_proto, type_context):
        normal_enum_type = normalize_type_context_name(type_context.name)
        anchor = format_anchor(enum_cross_ref_label(normal_enum_type))
        header = format_header('-', 'Enum %s' % normal_enum_type)
        proto_link = github_url(f"[{normal_enum_type} proto]", type_context) + '\n\n'
        leading_comment = type_context.leading_comment
        formatted_leading_comment = format_comment_with_annotations(leading_comment)
        if hide_not_implemented(leading_comment):
            return ''
        return anchor + header + proto_link + formatted_leading_comment + format_enum_as_definition_list(
            type_context, enum_proto)

    def visit_message(self, msg_proto, type_context, nested_msgs, nested_enums):
        # Skip messages synthesized to represent map types.
        if msg_proto.options.map_entry:
            return ''
        normal_msg_type = normalize_type_context_name(type_context.name)
        anchor = format_anchor(message_cross_ref_label(normal_msg_type))
        header = format_header('-', normal_msg_type)
        proto_link = github_url(f"[{normal_msg_type} proto]", type_context) + '\n\n'
        leading_comment = type_context.leading_comment
        formatted_leading_comment = format_comment_with_annotations(
            leading_comment,
            msg_proto.options.HasExtension(xds_status_pb2.message_status)
            and msg_proto.options.Extensions[xds_status_pb2.message_status].work_in_progress)
        if hide_not_implemented(leading_comment):
            return ''

        message = anchor + header + proto_link + formatted_leading_comment + format_message_as_json(
            type_context, msg_proto) + format_message_as_definition_list(
                type_context, msg_proto,
                self.protodoc_manifest) + '\n'.join(nested_msgs) + '\n' + '\n'.join(nested_enums)
        error = self.backticks_check(message)
        if error:
            logger.warning(f"Bad RST ({msg_proto.name}): {error}")
        return message

    def visit_file(self, file_proto, type_context, services, msgs, enums):
        # If there is a file-level 'not-implemented-hide' annotation then return empty string.
        if (annotations.NOT_IMPLEMENTED_HIDE_ANNOTATION
                in type_context.source_code_info.file_level_annotations):
            return ''

        has_messages = True
        if all(len(msg) == 0 for msg in msgs) and all(len(enum) == 0 for enum in enums):
            has_messages = False

        # TODO(mattklein123): The logic in both the doc and transform tool around files without messages
        # is confusing and should be cleaned up. This is a stop gap to have titles for all proto docs
        # in the common case.
        if (has_messages and not annotations.DOC_TITLE_ANNOTATION
                in type_context.source_code_info.file_level_annotations
                and file_proto.name.startswith('envoy')):
            raise ProtodocError(
                'Envoy API proto file missing [#protodoc-title:] annotation: {}'.format(
                    file_proto.name))

        # Find the earliest detached comment, attribute it to file level.
        # Also extract file level titles if any.
        header, comment = format_header_from_file(
            '=', type_context.source_code_info, file_proto.name)

        # If there are no messages, we don't include in the doc tree (no support for
        # service rendering yet). We allow these files to be missing from the
        # toctrees.
        if not has_messages:
            header = ':orphan:\n\n' + header
        warnings = ''
        added_wip_warning = False
        if file_proto.options.HasExtension(udpa_status_pb2.file_status):
            if file_proto.options.Extensions[udpa_status_pb2.file_status].work_in_progress:
                added_wip_warning = True
                warnings += WIP_WARNING
        if not added_wip_warning and file_proto.options.HasExtension(xds_status_pb2.file_status):
            if file_proto.options.Extensions[xds_status_pb2.file_status].work_in_progress:
                warnings += WIP_WARNING
        # debug_proto = format_proto_as_block_comment(file_proto)
        return header + warnings + comment + '\n'.join(msgs) + '\n'.join(enums)  # + debug_proto


def main():
    plugin.plugin([plugin.direct_output_descriptor('.rst', RstFormatVisitor)])


if __name__ == '__main__':
    main()
