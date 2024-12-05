#!/usr/bin/env bash

mkdir -p /etc/frr
mkdir -p /etc/supervisor/conf.d

CFGGEN_PARAMS=" \
    -d \
    -y /etc/sonic/constants.yml \
    -t /usr/share/sonic/templates/frr_vars.j2 \
    -t /usr/share/sonic/templates/supervisord/supervisord.conf.j2,/etc/supervisor/conf.d/supervisord.conf \
    -t /usr/share/sonic/templates/supervisord/critical_processes.j2,/etc/supervisor/critical_processes \
    -t /usr/share/sonic/templates/isolate.j2,/usr/sbin/bgp-isolate \
    -t /usr/share/sonic/templates/unisolate.j2,/usr/sbin/bgp-unisolate \
"

FRR_VARS=$(sonic-cfggen $CFGGEN_PARAMS)
CONFIG_TYPE=$(echo $FRR_VARS | jq -r '.docker_routing_config_mode')

update_default_gw()
{
   IP_VER=${1}
   # FRR is not running in host namespace so we need to delete
   # default gw kernel route added by docker network via eth0 and add it back
   # with higher administrative distance so that default route learnt
   # by FRR becomes best route if/when available
   GATEWAY_IP=$(ip -${IP_VER} route show default dev eth0 | awk '{print $3}')
   #Check if docker default route is there
   if [[ ! -z "$GATEWAY_IP" ]]; then
      ip -${IP_VER} route del default dev eth0
      #Make sure route is deleted
      CHECK_GATEWAY_IP=$(ip -${IP_VER} route show default dev eth0 | awk '{print $3}')
      if [[ -z "$CHECK_GATEWAY_IP" ]]; then
         # Ref: http://docs.frrouting.org/en/latest/zebra.html#zebra-vrf
         # Zebra does treat Kernel routes as special case for the purposes of Admin Distance. \
         # Upon learning about a route that is not originated by FRR we read the metric value as a uint32_t.
         # The top byte of the value is interpreted as the Administrative Distance and
         # the low three bytes are read in as the metric.
         # so here we are programming administrative distance of 210 (210 << 24) > 200 (for routes learnt via IBGP)
         ip -${IP_VER} route add default via $GATEWAY_IP dev eth0 metric 3523215360
      fi
   fi
}

if [[ ! -z "$NAMESPACE_ID" ]]; then
   update_default_gw 4
   update_default_gw 6
fi

if [ -z "$CONFIG_TYPE" ] || [ "$CONFIG_TYPE" == "separated" ]; then
    CFGGEN_PARAMS=" \
        -d \
        -y /etc/sonic/constants.yml \
        -t /usr/share/sonic/templates/bgpd/gen_bgpd.conf.j2,/etc/frr/bgpd.conf \
        -t /usr/share/sonic/templates/zebra/zebra.conf.j2,/etc/frr/zebra.conf \
        -t /usr/share/sonic/templates/staticd/gen_staticd.conf.j2,/etc/frr/staticd.conf \
    "
    MGMT_FRAMEWORK_CONFIG=$(echo $FRR_VARS | jq -r '.frr_mgmt_framework_config')
    if [ -n "$MGMT_FRAMEWORK_CONFIG" ] && [ "$MGMT_FRAMEWORK_CONFIG" != "false" ]; then
        CFGGEN_PARAMS="$CFGGEN_PARAMS \
            -t /usr/local/sonic/frrcfgd/bfdd.conf.j2,/etc/frr/bfdd.conf \
            -t /usr/local/sonic/frrcfgd/ospfd.conf.j2,/etc/frr/ospfd.conf \
        "
    else
        rm -f /etc/frr/bfdd.conf /etc/frr/ospfd.conf
    fi
    sonic-cfggen $CFGGEN_PARAMS
    echo "no service integrated-vtysh-config" > /etc/frr/vtysh.conf
    rm -f /etc/frr/frr.conf
elif [ "$CONFIG_TYPE" == "split" ]; then
    echo "no service integrated-vtysh-config" > /etc/frr/vtysh.conf
    rm -f /etc/frr/frr.conf
elif [ "$CONFIG_TYPE" == "split-unified" ]; then
    echo "service integrated-vtysh-config" > /etc/frr/vtysh.conf
    rm -f /etc/frr/bgpd.conf /etc/frr/zebra.conf /etc/frr/staticd.conf
elif [ "$CONFIG_TYPE" == "unified" ]; then
    CFGGEN_PARAMS=" \
        -d \
        -y /etc/sonic/constants.yml \
        -t /usr/share/sonic/templates/gen_frr.conf.j2,/etc/frr/frr.conf \
    "
    sonic-cfggen $CFGGEN_PARAMS # 这里有一个bug，issu:https://github.com/sonic-net/sonic-buildimage/issues/20019， 修复pr:https://github.com/sonic-net/sonic-buildimage/pull/20020
    echo "service integrated-vtysh-config" > /etc/frr/vtysh.conf
    if grep -qxF 'fpm address 127.0.0.1' /etc/frr/frr.conf; then
        echo 'fpm address 127.0.0.1 configured'
    else
        echo 'fpm address 127.0.0.1' >> /etc/frr/frr.conf
        echo '!' >> /etc/frr/frr.conf
    fi
    if grep -qxF 'no fpm use-next-hop-groups' /etc/frr/frr.conf; then
        echo 'no fpm use-next-hop-groups configured'
    else
        echo 'no fpm use-next-hop-groups' >> /etc/frr/frr.conf
        echo '!' >> /etc/frr/frr.conf
    fi
    rm -f /etc/frr/bgpd.conf /etc/frr/zebra.conf /etc/frr/staticd.conf \
          /etc/frr/bfdd.conf /etc/frr/ospfd.conf /etc/frr/pimd.conf
fi

chown -R frr:frr /etc/frr/

chown root:root /usr/sbin/bgp-isolate
chmod 0755 /usr/sbin/bgp-isolate

chown root:root /usr/sbin/bgp-unisolate
chmod 0755 /usr/sbin/bgp-unisolate

mkdir -p /var/sonic
echo "# Config files managed by sonic-config-engine" > /var/sonic/config_status

sysrepoctl -i /usr/share/yang/frr-vrf.yang
sysrepoctl -i /usr/share/yang/frr-interface.yang
sysrepoctl -i /usr/share/yang/frr-filter.yang
sysrepoctl -i /usr/share/yang/frr-route-map.yang
sysrepoctl -i /usr/share/yang/frr-route-types.yang
sysrepoctl -i /usr/share/yang/frr-isisd.yang

sysrepoctl -i /usr/share/yang/frr-bfdd.yang
sysrepoctl -i /usr/share/yang/ietf-routing-types.yang
sysrepoctl -i /usr/share/yang/frr-nexthop.yang
sysrepoctl -i /usr/share/yang/frr-pathd.yang

sysrepoctl -i /usr/share/yang/frr-bgp-filter.yang
sysrepoctl -i /usr/share/yang/frr-bgp-route-map.yang

sysrepoctl -i /usr/share/yang/frr-routing.yang
sysrepoctl -i /usr/share/yang/frr-bgp-types.yang
sysrepoctl -i /usr/share/yang/ietf-bgp-types.yang
sysrepoctl -i /usr/share/yang/frr-bgp.yang -s usr/share/yang
sysrepoctl -i /usr/share/yang/frr-bgp-common.yang
sysrepoctl -i /usr/share/yang/frr-bgp-common-structure.yang
sysrepoctl -i /usr/share/yang/frr-bgp-common-multiprotocol.yang
sysrepoctl -i /usr/share/yang/frr-bgp-neighbor.yang
sysrepoctl -i /usr/share/yang/frr-bgp-peer-group.yang
sysrepoctl -i /usr/share/yang/frr-bgp-bmp.yang

sysrepoctl -i /usr/share/yang/openconfig-extensions.yang
sysrepoctl -i /usr/share/yang/openconfig-inet-types.yang
sysrepoctl -i /usr/share/yang/openconfig-telemetry-types.yang
sysrepoctl -i /usr/share/yang/openconfig-telemetry.yang
sysrepoctl -i /usr/share/yang/cscn-extension.yang
sysrepoctl -i /usr/share/yang/cscn-network-instance.yang
sysrepoctl -i /usr/share/yang/cscn-pub-type.yang
sysrepoctl -i /usr/share/yang/cscn-openconfig-telemetry-ext.yang

sysrepoctl -c frr-filter -o frr -g frr -p 666
sysrepoctl -c frr-interface -o frr -g frr -p 666
sysrepoctl -c frr-isisd -o frr -g frr -p 666
sysrepoctl -c frr-route-map -o frr -g frr -p 666
sysrepoctl -c frr-route-types -o frr -g frr -p 666
sysrepoctl -c frr-vrf -o frr -g frr -p 666

sysrepoctl -c frr-bfdd -o frr -g frr -p 666
sysrepoctl -c ietf-routing-types -o frr -g frr -p 666
sysrepoctl -c frr-nexthop -o frr -g frr -p 666
sysrepoctl -c frr-pathd -o frr -g frr -p 666

sysrepoctl -c frr-bgp-filter -o root -g root -p 666
sysrepoctl -c frr-bgp-route-map -o root -g root -p 666

sysrepoctl -c frr-routing -o root -g root -p 666
sysrepoctl -c frr-bgp-types -o root -g root -p 666
sysrepoctl -c ietf-bgp-types -o root -g root -p 666
sysrepoctl -c frr-bgp -o root -g root -p 666

sysrepoctl -c frr-filter -p 666
sysrepoctl -c frr-interface -p 666
sysrepoctl -c frr-isisd -p 666
sysrepoctl -c frr-route-map -p 666
sysrepoctl -c frr-route-types -p 666
sysrepoctl -c frr-vrf -p 666

sysrepoctl -c frr-bfdd -p 666
sysrepoctl -c ietf-routing-types -p 666
sysrepoctl -c frr-nexthop -p 666
sysrepoctl -c frr-pathd -p 666

sysrepoctl -c frr-bgp-filter -p 666
sysrepoctl -c frr-bgp-route-map -p 666

sysrepoctl -c frr-routing -p 666
sysrepoctl -c frr-bgp-types -p 666
sysrepoctl -c ietf-bgp-types -p 666
sysrepoctl -c frr-bgp -p 666

sysrepoctl -c ietf-interfaces -o root -g root -p 666

sysrepoctl -c openconfig-extensions -o root -g root -p 666
sysrepoctl -c openconfig-inet-types -o root -g root -p 666
sysrepoctl -c openconfig-telemetry-types -o root -g root -p 666
sysrepoctl -c openconfig-telemetry -o root -g root -p 666
sysrepoctl -c cscn-extension -o root -g root -p 666
sysrepoctl -c cscn-network-instance -o root -g root -p 666
sysrepoctl -c cscn-pub-type -o root -g root -p 666
sysrepoctl -c cscn-openconfig-telemetry-ext -o root -g root -p 666

netopeer2-server -d -v 2 &

exec /usr/local/bin/supervisord
