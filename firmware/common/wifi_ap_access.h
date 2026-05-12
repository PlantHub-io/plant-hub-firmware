// wifi_ap_access.h
//
// Re-enables the WiFi AP after STA connects, so the captive portal stays
// reachable during provisioning.
//
// ESPHome 2026.x marked `esphome::wifi::WiFiComponent` `final`, so the
// previous subclass-hack in common/wifi.yaml no longer compiles. There is no
// public API to flip `ap_setup_` and call `setup_ap_config_()` from outside.
//
// We use the standard "friend injection via explicit template instantiation"
// idiom: explicit instantiations are exempt from access checks per the C++
// rule for explicit-instantiation contexts, so we can take pointer-to-member
// for protected members and surface them through an injected friend function.
#pragma once

#include "esphome/components/wifi/wifi_component.h"
#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace planthub {
namespace wifi_access {

struct ApSetupTag {
  using type = bool esphome::wifi::WiFiComponent::*;
  friend type get(ApSetupTag);
};

struct SetupApConfigTag {
  using type = void (esphome::wifi::WiFiComponent::*)();
  friend type get(SetupApConfigTag);
};

template <typename Tag, typename Tag::type V>
struct Inject {
  friend typename Tag::type get(Tag) { return V; }
};

template struct Inject<ApSetupTag, &esphome::wifi::WiFiComponent::ap_setup_>;
template struct Inject<SetupApConfigTag, &esphome::wifi::WiFiComponent::setup_ap_config_>;

}  // namespace wifi_access

inline void reenable_wifi_ap() {
  auto *c = esphome::wifi::global_wifi_component;
  if (c == nullptr) return;
  // Unqualified call — `get` is a hidden friend of the tag types, only
  // findable via ADL on the tag argument (which lives in wifi_access::).
  c->*get(wifi_access::ApSetupTag{}) = false;
  (c->*get(wifi_access::SetupApConfigTag{}))();
#ifdef USE_CAPTIVE_PORTAL
  if (esphome::captive_portal::global_captive_portal != nullptr) {
    esphome::captive_portal::global_captive_portal->start();
  }
#endif
}

}  // namespace planthub
