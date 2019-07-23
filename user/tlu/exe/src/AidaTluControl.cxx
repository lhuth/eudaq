#include "AidaTluController.hh"
#include "AidaTluHardware.hh"
#include "AidaTluPowerModule.hh"

#include <string>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <csignal>
#include <memory>
#include <iostream>
#include <ostream>
#include <vector>
#include <chrono>
#include <thread>

class AidaTluControl {
public:
    AidaTluControl();
    void DoConfigure();
    void DoInitialise();
    void SetPMTVoltage(double voltage);
    void SetTLUThreshold(double threshold);
    int DoMeasureRate(double voltage, double threshold, double time);

private:
    std::unique_ptr<tlu::AidaTluController> m_tlu;
    uint8_t m_verbose;
};

AidaTluControl::AidaTluControl(){
    m_verbose = 0x0;
}


//// zusammenfassen
// Initialize TLU
void AidaTluControl::DoInitialise(){

    /* Establish a connection with the TLU using IPBus.
       Define the main hardware parameters.
    */

    std::string uhal_conn = "file://../user/eudet/misc/hw_conf/aida_tlu/aida_tlu_address-fw_version_14.xml";
    std::string uhal_node = "fmctlu.udp";
    //std::string uhal_conn;
    //    std::string uhal_node;
    //    uhal_conn = ini->Get("ConnectionFile", uhal_conn);
    //    uhal_node = ini->Get("DeviceName",uhal_node);
    m_tlu = std::unique_ptr<tlu::AidaTluController>(new tlu::AidaTluController(uhal_conn, uhal_node));

    m_verbose = 0x1;


    // Import I2C addresses for hardware
    // Populate address list for I2C elements
    //// What of this do I need?
//    m_tlu->SetI2C_core_addr(ini->Get("I2C_COREEXP_Addr", 0x21));
//    m_tlu->SetI2C_clockChip_addr(ini->Get("I2C_CLK_Addr", 0x68));
//    m_tlu->SetI2C_DAC1_addr(ini->Get("I2C_DAC1_Addr",0x13) );
//    m_tlu->SetI2C_DAC2_addr(ini->Get("I2C_DAC2_Addr",0x1f) );
//    m_tlu->SetI2C_EEPROM_addr(ini->Get("I2C_ID_Addr", 0x50) );
//    m_tlu->SetI2C_expander1_addr(ini->Get("I2C_EXP1_Addr",0x74));
//    m_tlu->SetI2C_expander2_addr(ini->Get("I2C_EXP2_Addr",0x75) );
//    m_tlu->SetI2C_pwrmdl_addr(ini->Get("I2C_DACModule_Addr",  0x1C), ini->Get("I2C_EXP1Module_Addr",  0x76), ini->Get("I2C_EXP2Module_Addr",  0x77), ini->Get("I2C_pwrId_Addr",  0x51));
//    m_tlu->SetI2C_disp_addr(ini->Get("I2C_disp_Addr",0x3A));

    // Initialize TLU hardware
    //// What happens here?
    m_tlu->InitializeI2C(m_verbose);
    m_tlu->InitializeIOexp(m_verbose);
    //    if (ini->Get("intRefOn", false)){
    //        m_tlu->InitializeDAC(ini->Get("intRefOn", false), ini->Get("VRefInt", 2.5), m_verbose);
    //    }
    //    else{
    //        m_tlu->InitializeDAC(ini->Get("intRefOn", false), ini->Get("VRefExt", 1.3), m_verbose);
    //    }

    //    // Initialize the Si5345 clock chip using pre-generated file
    //    if (ini->Get("CONFCLOCK", true)){
    //        std::string  clkConfFile;
    //        std::string defaultCfgFile= "./../user/eudet/misc/hw_conf/aida_tlu/fmctlu_clock_config.txt";
    //        clkConfFile= ini->Get("CLOCK_CFG_FILE", defaultCfgFile);
    //        if (clkConfFile== defaultCfgFile){
    //            EUDAQ_WARN("TLU: Could not find the parameter for clock configuration in the INI file. Using the default.");
    //        }
    //        int clkres;
    //        clkres= m_tlu->InitializeClkChip( clkConfFile, m_verbose  );
    //        if (clkres == -1){
    //            EUDAQ_ERROR("TLU: clock configuration failed.");
    //        }
    //    }

    //    // Reset IPBus registers
    //    m_tlu->ResetSerdes();
    //    m_tlu->ResetCounters();
    //    m_tlu->SetTriggerVeto(1, m_verbose);
    //    m_tlu->ResetFIFO();
    //    m_tlu->ResetEventsBuffer();

    //    m_tlu->ResetTimestamp();




}

// Configure TLU
void AidaTluControl::DoConfigure(){
    /*
    auto conf = GetConfiguration();

    EUDAQ_INFO("CONFIG ID: " + std::to_string(conf->Get("confid", 0)));
    m_verbose = abs(conf->Get("verbose", 0));
    EUDAQ_INFO("TLU VERBOSITY SET TO: " + std::to_string(m_verbose));

    //Set lemo clock
    //// What exactly is set here?
    if(m_verbose > 0) EUDAQ_INFO(" -CLOCK OUTPUT CONFIGURATION");
    m_tlu->enableClkLEMO(conf->Get("LEMOclk", true), m_verbose);

    if(m_verbose > 0) EUDAQ_INFO(" -SHUTTER OPERATION MODE");
    m_tlu->SetShutterParameters( (bool)conf->Get("EnableShutterMode",0),
                                 (int8_t)(conf->Get("ShutterSource",0)),
                                 (int32_t)(conf->Get("ShutterOnTime",0)),
                                 (int32_t)(conf->Get("ShutterOffTime",0)),
                                 (int32_t)(conf->Get("ShutterVetoOffTime",0)),
                                 (int32_t)(conf->Get("InternalShutterInterval",0)),
                                  m_verbose);


    if(m_verbose > 0) EUDAQ_INFO(" -AUTO TRIGGER SETTINGS");
    m_tlu->SetInternalTriggerFrequency( (int32_t)( conf->Get("InternalTriggerFreq", 0)), m_verbose );

    if(m_verbose > 0) EUDAQ_INFO(" -FINALIZING TLU CONFIGURATION");
    m_tlu->SetEnableRecordData( (uint32_t)(conf->Get("EnableRecordData", 1)) );
    m_tlu->GetEventFifoCSR();
    m_tlu->GetEventFifoFillLevel();
*/
}

// Set PMT Voltage
void AidaTluControl::SetPMTVoltage(double val){
    m_tlu->pwrled_setVoltages(val, val, val, val, m_verbose);
}

// Set TLU threshold
void AidaTluControl::SetTLUThreshold(double val){
    m_tlu->SetThresholdValue(0, val , m_verbose);
    m_tlu->SetThresholdValue(1, val , m_verbose);
    m_tlu->SetThresholdValue(2, val , m_verbose);
    m_tlu->SetThresholdValue(3, val , m_verbose);
    m_tlu->SetThresholdValue(4, val , m_verbose);
    m_tlu->SetThresholdValue(5, val , m_verbose);
}


// Measure rate
int AidaTluControl::DoMeasureRate(double voltage, double threshold, double time){
    SetPMTVoltage(voltage);
    SetTLUThreshold(threshold);
    // get rate for time
    // return rate
}

int main(int /*argc*/, char **argv) {
    // array of voltages, array of threshold
    // dictionary for rates
    // for loop over threshold, save return of DoMeasureRate in dict
    AidaTluControl myTlu;
    myTlu.DoInitialise();
    myTlu.DoConfigure();

    myTlu.SetPMTVoltage(0.5);
}


/* I need:  - Init
            - Config
            - voltage setter
            - threshold setter
            - data collector
*/
