#include "eudaq/StdEventConverter.hh"
#include "eudaq/RawEvent.hh"

#include "library/telescope_frame.hpp"

class MuPix8RawEvent2StdEventConverter: public eudaq::StdEventConverter{
public:
  bool Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const override;
  static const uint32_t m_id_factory = eudaq::cstr2hash("ATLASPix");
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<MuPix8RawEvent2StdEventConverter>(MuPix8RawEvent2StdEventConverter::m_id_factory);
}

bool MuPix8RawEvent2StdEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const{
  auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);
  auto block_n_list = ev->GetBlockNumList();
  auto dutType = conf->Get("sensor_id",8);
  for(auto &block_n: block_n_list){
    auto block =  ev->GetBlock(block_n);
    TelescopeFrame * tf = new TelescopeFrame();
    if(!tf->from_uint8_t(ev->GetBlock(block_n)))
        EUDAQ_ERROR("Cannot read TelescopeFrame");


    eudaq::StandardPlane plane(0, "atlaspix", "atlaspix");

    plane.SetSizeZS(512,512,tf->num_hits());
    for(uint i =0; i < tf->num_hits();++i)
    {
        RawHit h = tf->get_hit(i,dutType);
        plane.SetPixel(0,h.column(),h.row(),d1->GetTimestampBegin()*1000);
    }
    d2->AddPlane(plane);
  }
  return true;
}
