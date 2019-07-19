#include "eudaq/Producer.hh"
       #include <unistd.h>


class SpeedTestProducer : public eudaq::Producer {
public:
  SpeedTestProducer(const std::string name, const std::string &runcontrol);
  ~SpeedTestProducer() override;
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoReset() override;
  void DoTerminate() override;
  void RunLoop() override;

  static const uint32_t m_id_factory = eudaq::cstr2hash("SpeedTestProducer");
private:
  bool m_running;
  int eventID;
  int m_sleepingTime;
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::
    Register<SpeedTestProducer, const std::string&, const std::string&>(SpeedTestProducer::m_id_factory);
}

SpeedTestProducer::SpeedTestProducer(const std::string name, const std::string &runcontrol)
  : eudaq::Producer(name, runcontrol), m_running(false), eventID(0), m_sleepingTime(100){
  m_running = false;
}

SpeedTestProducer::~SpeedTestProducer(){
  m_running = false;
}

void SpeedTestProducer::RunLoop(){
 while(m_running)
 {
    std::vector<uint32_t> data;
    data.push_back(0x015);
    data.push_back(0x014);
    data.push_back(0x013);
    data.push_back(0x012);
    data.push_back(0x011);
    data.push_back(0x02);
    data.push_back(0x03);
    data.push_back(0x04);
    data.push_back(0x05);
    data.push_back(0x06);
    data.push_back(0x07);
    data.push_back(0x08);
    data.push_back(0x09);
    data.push_back(0x010);
    data.push_back(0x011);
    auto ev = eudaq::Event::MakeUnique("Ex0Raw");
    ev->AddBlock(0,data);
    ev->AddBlock(1,data);
    ev->AddBlock(2,data);
    ev->AddBlock(3,data);
    ev->SetEventN(eventID);
    eventID++;
    SendEvent(std::move(ev));
    if(eventID%100==0)
        usleep(m_sleepingTime);
 }
}

void SpeedTestProducer::DoInitialise(){
  auto conf = GetInitConfiguration();
}

void SpeedTestProducer::DoConfigure(){
  auto conf = GetConfiguration();
  if(conf->Has("sleep"))
      EUDAQ_USER("sleep found");
  m_sleepingTime = conf->Get("sleep",100);
  
}

void SpeedTestProducer::DoStartRun(){
  m_running = true;
   eventID =0;
   std::string info = std::to_string(m_sleepingTime);
   EUDAQ_USER(info);
}

void SpeedTestProducer::DoStopRun(){
  m_running = false;
  eventID =0;
}

void SpeedTestProducer::DoReset(){
  m_running = false;
}

void SpeedTestProducer::DoTerminate() {
  m_running = false;
}
