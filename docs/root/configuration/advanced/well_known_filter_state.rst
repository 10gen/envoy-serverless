.. _well_known_filter_state:

Well Known Filter State Objects
===============================

The following list of filter state objects are consumed by Envoy extensions:

``envoy.network.upstream_server_name``
  Sets the transport socket option to override the `SNI <https://en.wikipedia.org/wiki/Server_Name_Indication>`_ in
  the upstream connections. Accepts a host name as a constructor, e.g. "lyft.com".

``envoy.network.application_protocols``
  Sets the transport socket option to override the `ALPN <https://en.wikipedia.org/wiki/Application-Layer Protocol
  Negotiation>`_ list in the upstream connections. This setting takes precedence over the upstream cluster configuration.
  Accepts a comma-separated list of protocols as a constructor, e.g. "h2,http/1.1".

``envoy.network.upstream_subject_alt_names``
  Enables additional verification of the upstream peer certificate SAN names. Accepts a comma-separated list of SAN
  names as a constructor.

``envoy.tcp_proxy.cluster``
  :ref:`TCP proxy <config_network_filters_tcp_proxy>` dynamic cluster name selection on a per-connection basis. Accepts
  a cluster name as a constructor.

``envoy.network.transport_socket.original_dst_address``
  :ref:`Original destination cluster <arch_overview_load_balancing_types_original_destination>` dynamic address
  selection. Accepts an `IP:PORT` string as a constructor. Fields:

  * ``ip``: IP address value as a string;
  * ``port``: port value as a number.

``envoy.filters.listener.original_dst.local_ip``
  :ref:`Original destination listener filter <config_listener_filters_original_dst>` destination address selection for
  the internal listeners. Accepts an `IP:PORT` string as a constructor. Fields:

  * ``ip``: IP address value as a string;
  * ``port``: port value as a number.

``envoy.filters.listener.original_dst.remote_ip``
  :ref:`Original destination listener filter <config_listener_filters_original_dst>` source address selection for the
  internal listeners. Accepts an `IP:PORT` string as a constructor. Fields:

  * ``ip``: IP address value as a string;
  * ``port``: port value as a number.

``envoy.upstream.dynamic_host``
  :ref:`Dynamic forward proxy <envoy_v3_api_msg_extensions.clusters.dynamic_forward_proxy.v3.ClusterConfig>` upstream
  host override on a per-connection basis. Accepts a host string as a constructor.

``envoy.upstream.dynamic_port``
  :ref:`Dynamic forward proxy <envoy_v3_api_msg_extensions.clusters.dynamic_forward_proxy.v3.ClusterConfig>` upstream
  port override on a per-connection basis. Accepts a port number string as a constructor.

``envoy.tcp_proxy.disable_tunneling``
  :ref:`TCP proxy tunneling override
  <envoy_v3_api_field_extensions.filters.network.tcp_proxy.v3.TcpProxy.tunneling_config>` to disable tunneling on a
  per-connection bases. Accepts values "true" and "false".


Filter state object fields
--------------------------

The filter state object fields can be used in the format strings. For example,
the following format string references the port number in the original
destination cluster filter state object:

.. code-block:: none

  %FILTER_STATE(envoy.network.transport_socket.original_dst_address:FIELD:port)%
