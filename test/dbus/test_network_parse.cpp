
#include <cassert>

#include "service/network_service.h"

void test_network_parse() {
    {
        std::string text = "HomeNet:WPA2:80:*\n"
                           "Guest:WPA2 WPA3:40:\n"
                           "Open:--:20:\n";
        std::set<std::string> existing;
        auto nets = network_parse_networks(text, existing);
        assert(nets.size() == 3);
        assert(nets.at("HomeNet").connected);
        assert(nets.at("HomeNet").signal == 80);
        assert(nets.at("HomeNet").security == "WPA2");
        assert(nets.at("Guest").security == "WPA2/WPA3");
        assert(!nets.at("Guest").connected);
        assert(nets.at("Open").security == "--");
        for (const auto &[ssid, info] : nets)
            assert(info.in_range);
    }

    {
        std::string text = "Dup:WPA2:30:\n"
                           "Dup:WPA2:90:*\n";
        auto nets = network_parse_networks(text, {});
        assert(nets.size() == 1);
        assert(nets.at("Dup").connected);
    }

    {
        std::string text = "My\\:Network:WPA2:50:\n";
        auto nets = network_parse_networks(text, {});
        assert(nets.size() == 1);
        assert(nets.count("My:Network") == 1);
    }

    {
        std::set<std::string> existing = {"SavedNet"};
        auto nets = network_parse_networks("", existing);
        assert(nets.size() == 1);
        const NetworkInfo &info = nets.at("SavedNet");
        assert(info.existing);
        assert(!info.in_range);
        assert(!info.connected);
    }

    {
        auto nets = network_parse_networks("garbage:noenoughfields\n", {});
        assert(nets.empty());
    }

    {
        std::string text = "wlan0:wifi:connected:MyWifi\n"
                           "eth0:ethernet:connected:Wired connection 1\n"
                           "lo:loopback:unmanaged:--\n";
        auto st = network_parse_device_status(text);
        assert(st.wifi);
        assert(st.ethernet);
        assert(st.ethernet_connected);
        assert(st.ethernet_name == "Wired connection 1");
    }
    {
        std::string text = "eth0:ethernet:connecting:--\n";
        auto st = network_parse_device_status(text);
        assert(st.ethernet);
        assert(!st.ethernet_connected);
    }
    {
        std::string text = "wlan0:wifi:unavailable:--\n";
        auto st = network_parse_device_status(text);
        assert(st.wifi);
        assert(!st.ethernet);
    }

    {
        std::string text = "HomeNet:802-11-wireless\n"
                           "Wired connection 1:802-3-ethernet\n"
                           "Guest:802-11-wireless\n";
        auto profiles = network_parse_profiles(text);
        assert(profiles.size() == 2);
        assert(profiles.count("HomeNet") == 1);
        assert(profiles.count("Guest") == 1);
        assert(profiles.count("Wired connection 1") == 0);
    }

    {
        NetworkState state;
        NetworkInfo connected;
        connected.connected = true;
        state.networks["A"] = connected;
        NetworkInfo saved_in_range;
        saved_in_range.existing = true;
        saved_in_range.in_range = true;
        state.networks["B"] = saved_in_range;
        NetworkInfo available;
        available.existing = false;
        state.networks["C"] = available;
        NetworkInfo stub;
        stub.existing = true;
        stub.in_range = false;
        state.networks["D"] = stub;
        assert(network_visible_count(state) == 3);
    }

    {
        NetworkState populated;
        NetworkInfo a, b, c;
        a.connected = true;
        a.in_range = true;
        b.existing = true;
        b.in_range = true;
        c.in_range = true;
        populated.networks["A"] = a;
        populated.networks["B"] = b;
        populated.networks["C"] = c;

        std::map<std::string, NetworkInfo> self_only;
        NetworkInfo mine;
        mine.connected = true;
        mine.in_range = true;
        self_only["A"] = mine;
        assert(network_scan_would_collapse(populated, self_only));

        std::map<std::string, NetworkInfo> neighbour_only;
        NetworkInfo other;
        other.in_range = true;
        neighbour_only["Z"] = other;
        assert(!network_scan_would_collapse(populated, neighbour_only));

        std::map<std::string, NetworkInfo> full;
        full["A"] = a;
        full["B"] = b;
        assert(!network_scan_would_collapse(populated, full));

        NetworkState sparse;
        sparse.networks["Only"] = mine;
        assert(!network_scan_would_collapse(sparse, self_only));

        NetworkState st;
        st.networks = network_parse_networks("HomeNet:WPA2:80:*\n", {});
        assert(
            (network_parse_networks("HomeNet:WPA2:80:*\n", {}) == st.networks));
    }

    {
        NetworkState state;
        assert(state.connected_ssid() == "");
        assert(state.connected_signal() == 0);
        NetworkInfo info;
        info.connected = true;
        info.signal = 66;
        state.networks["X"] = info;
        assert(state.connected_ssid() == "X");
        assert(state.connected_signal() == 66);
    }
}
