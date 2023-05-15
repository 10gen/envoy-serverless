#pragma once

#include <string>

#include "envoy/config/typed_config.h"
#include "envoy/registry/registry.h"

#include "source/common/formatter/substitution_formatter.h"
#include "source/extensions/filters/common/expr/evaluator.h"

namespace Envoy {
namespace Extensions {
namespace Formatter {

class CELFormatter : public ::Envoy::Formatter::FormatterProvider {
public:
  CELFormatter(Extensions::Filters::Common::Expr::Builder&,
               const google::api::expr::v1alpha1::Expr&, absl::optional<size_t>&);

  absl::optional<std::string> format(const Http::RequestHeaderMap&, const Http::ResponseHeaderMap&,
                                     const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&,
                                     absl::string_view, AccessLog::AccessLogType) const override;
  ProtobufWkt::Value formatValue(const Http::RequestHeaderMap&, const Http::ResponseHeaderMap&,
                                 const Http::ResponseTrailerMap&, const StreamInfo::StreamInfo&,
                                 absl::string_view, AccessLog::AccessLogType) const override;

private:
  const google::api::expr::v1alpha1::Expr parsed_expr_;
  const absl::optional<size_t> max_length_;
  Extensions::Filters::Common::Expr::ExpressionPtr compiled_expr_;
};

class CELFormatterCommandParser : public ::Envoy::Formatter::CommandParser {
public:
  CELFormatterCommandParser()
      : expr_builder_(Extensions::Filters::Common::Expr::createBuilder(nullptr)){};
  ::Envoy::Formatter::FormatterProviderPtr parse(const std::string& command,
                                                 const std::string& subcommand,
                                                 absl::optional<size_t>& max_length) const override;

private:
  Extensions::Filters::Common::Expr::BuilderPtr expr_builder_;
};

} // namespace Formatter
} // namespace Extensions
} // namespace Envoy
