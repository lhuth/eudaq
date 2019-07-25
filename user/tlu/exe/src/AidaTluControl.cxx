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
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <map>

//#include "gnuplot-iostream.h"

class AidaTluControl {
public:
    AidaTluControl();
    void DoConfigure();
    void DoStartUp();
    void SetPMTVoltage(double voltage);
    void SetTLUThreshold(double threshold);
    void Test();
    std::vector<uint32_t> MeasureRate(double voltage, double threshold, int time);

private:
    std::unique_ptr<tlu::AidaTluController> m_tlu;
    uint8_t m_verbose;
    //    bool m_exit_of_run;
};

AidaTluControl::AidaTluControl(){
    m_verbose = 0x0;
}


// Initialize TLU
void AidaTluControl::DoStartUp(){

    /* Establish a connection with the TLU using IPBus.
       Define the main hardware parameters.
    */

    std::string uhal_conn = "file:///opt/eudaq2/user/eudet/misc/hw_conf/aida_tlu/aida_tlu_connection.xml";

    std::string uhal_node = "aida_tlu.controlhub";
    //std::string uhal_conn;
    //    std::string uhal_node;
    //    uhal_conn = ini->Get("ConnectionFile", uhal_conn);
    //    uhal_node = ini->Get("DeviceName",uhal_node);
    m_tlu = std::unique_ptr<tlu::AidaTluController>(new tlu::AidaTluController(uhal_conn, uhal_node));

    m_verbose = 0x1;


    // Populate address list for I2C elements
    m_tlu->SetI2C_core_addr(0x21);
    m_tlu->SetI2C_clockChip_addr(0x68);
    m_tlu->SetI2C_DAC1_addr(0x13);
    m_tlu->SetI2C_DAC2_addr(0x1f);
    m_tlu->SetI2C_EEPROM_addr(0x50);
    m_tlu->SetI2C_expander1_addr(0x74);
    m_tlu->SetI2C_expander2_addr(0x75);
    m_tlu->SetI2C_pwrmdl_addr(0x1C, 0x76, 0x77, 0x51);
    m_tlu->SetI2C_disp_addr(0x3A);

    // Initialize TLU hardware
    m_tlu->InitializeI2C(m_verbose);
    m_tlu->InitializeIOexp(m_verbose);
    m_tlu->InitializeDAC(false, 1.3, m_verbose);


    // Initialize the Si5345 clock chip using pre-generated file
    std::string defaultCfgFile= "file:///opt/eudaq2/user/eudet/misc/hw_conf/aida_tlu/fmctlu_clock_config.txt";
    int clkres;
    clkres= m_tlu->InitializeClkChip( defaultCfgFile, m_verbose  );
    if (clkres == -1){
        std::cout << "TLU: clock configuration failed." << std::endl;
    }


    // Reset IPBus registers
    m_tlu->ResetSerdes();
    m_tlu->ResetCounters();

    m_tlu->SetTriggerVeto(1, m_verbose); // no triggers
    m_tlu->ResetFIFO();
    m_tlu->ResetEventsBuffer();

    m_tlu->ResetTimestamp();



    m_tlu->enableClkLEMO(true, m_verbose);

    m_tlu->SetEnableRecordData((uint32_t)1);
    m_tlu->GetEventFifoCSR();
    m_tlu->GetEventFifoFillLevel();


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


*/
}

// Set PMT Voltage
void AidaTluControl::SetPMTVoltage(double val){
    m_tlu->pwrled_setVoltages(val, val, val, val, m_verbose);
}

// Set TLU threshold
void AidaTluControl::SetTLUThreshold(double val){
    std::cout << val << std::endl;
    m_tlu->SetThresholdValue(7, val , m_verbose); //all channels
}


// Measure rate
std::vector<uint32_t> AidaTluControl::MeasureRate(double voltage, double threshold, int time){
    SetPMTVoltage(voltage);
    SetTLUThreshold(threshold);
    m_tlu->SetRunActive(1, 1); // reset internal counters
    m_tlu->SetTriggerVeto(0, m_verbose); //enable trigger
    m_tlu->ReceiveEvents(m_verbose);
    std::this_thread::sleep_for (std::chrono::seconds(time));
    uint32_t sl0, sl1, sl2, sl3, sl4, sl5;
    m_tlu->GetScaler(sl0, sl1, sl2, sl3, sl4, sl5);
    std::cout << sl0 << "  " << sl1<< "  " << sl2<< "  " << sl3<< "  " << sl4<< "  " << sl5 << std::endl;

    m_tlu->SetTriggerVeto(1, m_verbose);
    // Set TLU internal logic to stop.
    m_tlu->SetRunActive(0, 1);
    m_tlu->ResetCounters();
    m_tlu->ResetSerdes();

    return {sl0, sl1, sl2, sl3, sl4, sl5};
}

void AidaTluControl::Test(){
    SetPMTVoltage(1);
    SetTLUThreshold(-0.04);
    m_tlu->SetRunActive(1, 1); // reset internal counters
    m_tlu->SetTriggerVeto(0, m_verbose); //enable trigger
    m_tlu->ReceiveEvents(m_verbose);
    std::this_thread::sleep_for (std::chrono::seconds(10));


    for (int i; i<10; i++){
        //tlu::fmctludata *data = m_tlu->PopFrontEvent();
        //std::cout << data->input0 << "  " << data->input1<< "  " << data->input2<< "  " << data->input3<< "  " << data->input4<< "  " << data->input5 << std::endl;


        uint32_t sl0, sl1, sl2, sl3, sl4, sl5, post, pt;
        post = m_tlu->GetPostVetoTriggers();
        pt=m_tlu->GetPreVetoTriggers();
        m_tlu->GetScaler(sl0, sl1, sl2, sl3, sl4, sl5);
        std::cout << sl0 << "  " << sl1<< "  " << sl2<< "  " << sl3<< "  " << sl4<< "  " << sl5 << std::endl;
        //std::cout << post << std::endl;
        //std::cout << pt << std::endl;
        std::this_thread::sleep_for (std::chrono::seconds(1));
    }
    m_tlu->SetTriggerVeto(1, m_verbose);
    // Set TLU internal logic to stop.
    m_tlu->SetRunActive(0, 1);
}

int main(int /*argc*/, char **argv) {
    // array of threshold

    // Threshold in [-1.3V,=1.3V] with 40e-6V presision
    //double thresholdMin = -1e-3;
    //double thresholdMax = -50e-3;
    //double thresholdDifference = thresholdMax - thresholdMin;

    //double thresholds[numberOfValues];

    //    for (int i = 0; i < numberOfValues; i++){
    //        thresholds[i] = thresholdMin + i * thresholdDifference / numberOfValues;
    //    }
    const int numberOfValues = 10;
    int time = 30; //time in seconds
    double voltage = 0.9;
    double thresholds[10] = {4, 5, 6, 7, 8, 9,10,20,30,40}; //values in mV
    for (int i = 0; i < 10; i++){
        thresholds[i] *= -1e-3;
    }


    // Get Rates:
    AidaTluControl myTlu;
    std::vector<std::vector<uint32_t>> rates(numberOfValues, std::vector<uint32_t>(6));

    myTlu.DoStartUp();
    std::ofstream outFile;
    outFile.open ("output.txt");

    for (int i = 0; i < numberOfValues; i++){
        rates[i] = myTlu.MeasureRate(voltage, thresholds[i], time);
        std::cout << "Threshold: " << thresholds[i]*1e3 << "mV" << std::endl;
        //std::cout << "Counts:";
        //for (auto r:rates) std::cout << r << "   ";
        //std::cout << std::endl;
        //std::cout <<rates[i][0] << std::endl;
        std::cout << "_______________________" << std::endl;

        for (auto r:rates[i]) outFile << r << ",";
        outFile << "\n";

    }
    outFile.close();

//    Gnuplot gp;

//    gp << "set xrange [-2:2]\nset yrange [-2:2]\n";
//            // '-' means read from stdin.  The send1d() function sends data to gnuplot's stdin.
//            gp << "plot '-' with vectors title 'pts_A'\n";
//            gp.send1d(pts_A);


    return 1;



    //    // for loop over threshold, save return of DoMeasureRate in dict
    //    for(int i = 0; i < numberOfValues; i++){
    //        rates[thresholds[i]] = myTlu.DoMeasureRate(voltage, thresholds[i], time);
    //    }
}


/* I need:  - Init
            - Config
            - voltage setter
            - threshold setter
            - data collector
*/
