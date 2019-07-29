#include "AidaTluController.hh"
#include "AidaTluHardware.hh"
#include "AidaTluPowerModule.hh"
#include "eudaq/OptionParser.hh"

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

// ROOT includes
//#include <TROOT.h>
//#include <TGraph.h>

//#include "gnuplot-iostream.h"

class AidaTluControl {
public:
    AidaTluControl();
    void DoStartUp();
    void SetPMTVoltage(double voltage);
    void SetTLUThreshold(double threshold);
    //    void Test();
    std::vector<uint32_t> MeasureRate(double voltage, double threshold, int time);

private:
    std::unique_ptr<tlu::AidaTluController> m_tlu;
    uint8_t m_verbose;
    uint64_t m_starttime;
    uint64_t m_lasttime;
    int m_nTrgIn;

    double m_duration;
    //    bool m_exit_of_run;
};

AidaTluControl::AidaTluControl(){
    m_verbose = 0x0;
    m_duration = 0;
    m_starttime = 0;
    m_lasttime = 0;
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
    m_tlu->DefineConst(0, 6); // 0 DUTs, 6 Triggers



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
    //    std::string defaultCfgFile= "file:///opt/eudaq2/user/eudet/misc/hw_conf/aida_tlu/fmctlu_clock_config.txt";
    //    int clkres;
    //    clkres= m_tlu->InitializeClkChip( defaultCfgFile, m_verbose  );
    //    if (clkres == -1){
    //        std::cout << "TLU: clock configuration failed." << std::endl;
    //    }

    // Set trigger stretch

    std::vector<unsigned int> stretcVec = {(unsigned int) 10,
                                           (unsigned int) 10,
                                           (unsigned int) 10,
                                           (unsigned int) 10,
                                           (unsigned int) 10,
                                           (unsigned int) 10};
    m_tlu->SetPulseStretchPack(stretcVec, m_verbose);


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

    uint32_t sl0=0, sl1=0, sl2=0, sl3=0, sl4=0, sl5=0;

    SetPMTVoltage(voltage);
    SetTLUThreshold(threshold);
    std::this_thread::sleep_for (std::chrono::seconds(1));
    m_tlu->ResetCounters();
    m_tlu->ResetSerdes();



    m_tlu->SetRunActive(1, 1); // reset internal counters
    m_starttime = m_tlu->GetCurrentTimestamp()*25;
    m_tlu->SetTriggerVeto(0, m_verbose); //enable trigger
    m_tlu->ReceiveEvents(m_verbose);

    std::this_thread::sleep_for (std::chrono::milliseconds(time*1000));

    m_tlu->SetTriggerVeto(1, m_verbose); //disable trigger
    // Set TLU internal logic to stop.
    m_tlu->SetRunActive(0, 1);
    m_lasttime = m_tlu->GetCurrentTimestamp()*25;

    m_tlu->GetScaler(sl0, sl1, sl2, sl3, sl4, sl5);


    std::cout << std::dec << sl0 << "  " << sl1<< "  " << sl2<< "  " << sl3<< "  " << sl4<< "  " << sl5 << std::endl;

    m_tlu->ResetCounters();
    m_tlu->ResetSerdes();



    m_duration = double(m_lasttime - m_starttime) / 1000000000; // in seconds
    //    std::cout << "Start" << double(m_starttime)/ 1000000000 << std::endl;
    //    std::cout << "Stop" << double(m_lasttime)/ 1000000000 << std::endl;
    std::cout << "Run duration [s]" << m_duration << std::endl;

    return {sl0, sl1, sl2, sl3, sl4, sl5};
}


//AidaTluControl::PlotData(std::vector<double> thresholds, std::vector rate){

//}

//AidaTluControl::TestPlot(){
//    Int_t n = 20;
//    Double_t x[n], y[n];
//    for (Int_t i = 0; i < n; i++){
//        x[i] = i*0.1;
//        y[i] = 10 * sin(x[i]=0.2);
//    }

//    TGra
//}

//void AidaTluControl::Test(){
//    SetPMTVoltage(1);
//    SetTLUThreshold(-0.04);
//    m_tlu->SetRunActive(1, 1); // reset internal counters
//    m_tlu->SetTriggerVeto(0, m_verbose); //enable trigger
//    m_tlu->ReceiveEvents(m_verbose);
//    std::this_thread::sleep_for (std::chrono::seconds(10));


//    for (int i; i<10; i++){
//        //tlu::fmctludata *data = m_tlu->PopFrontEvent();
//        //std::cout << data->input0 << "  " << data->input1<< "  " << data->input2<< "  " << data->input3<< "  " << data->input4<< "  " << data->input5 << std::endl;


//        uint32_t sl0, sl1, sl2, sl3, sl4, sl5, post, pt;
//        post = m_tlu->GetPostVetoTriggers();
//        pt=m_tlu->GetPreVetoTriggers();
//        m_tlu->GetScaler(sl0, sl1, sl2, sl3, sl4, sl5);
//        std::cout << sl0 << "  " << sl1<< "  " << sl2<< "  " << sl3<< "  " << sl4<< "  " << sl5 << std::endl;
//        //std::cout << post << std::endl;
//        //std::cout << pt << std::endl;
//        std::this_thread::sleep_for (std::chrono::seconds(1));
//    }
//    m_tlu->SetTriggerVeto(1, m_verbose);
//    // Set TLU internal logic to stop.
//    m_tlu->SetRunActive(0, 1);
//}



int main(int /*argc*/, char **argv) {
    eudaq::OptionParser op("EUDAQ Command Line FileReader modified for TLU", "2.1", "EUDAQ FileReader (TLU)");
    eudaq::Option<double> thrMin(op, "tl", "thresholdlow", -1.3, "double", "threshold value low [V]");
    eudaq::Option<double> thrMax(op, "th", "thresholdhigh", -0.1, "double", "threshold value high [V]");
    eudaq::Option<int> thrNum(op, "tn", "thresholdsteps", 9, "int", "number of threshold steps");
    eudaq::Option<double> volt(op, "v", "pmtvoltage", 0.9, "double", "PMT voltage [V]");
    eudaq::Option<int> acqtime(op, "t", "acquisitiontime", 10, "int", "acquisition time");

    try{
        op.Parse(argv);
    }
    catch (...) {
        return op.HandleMainException();
    }

    // array of threshold

    // Threshold in [-1.3V,=1.3V] with 40e-6V presision
    double thresholdMin = thrMin.Value();
    double thresholdMax = thrMax.Value();
    double thresholdDifference = thresholdMax - thresholdMin;
    const int numThresholdValues = thrNum.Value();

    double thresholds[numThresholdValues];

    if (numThresholdValues < 2) thresholds[0] = thresholdMin;
    else{
        for (int i = 0; i < numThresholdValues; i++){
            thresholds[i] = thresholdMin + i * thresholdDifference / (numThresholdValues-1);
        }
    }

    // const int numThresholdValues = 10;
    int time = acqtime.Value(); //time in seconds
    double voltage = volt.Value();
    //    double thresholds[10] = {4, 5, 6, 7, 8, 9,10,20,30,40}; //values in mV
    //    double thresholds[] = {
    //    for (int i = 0; i < 10; i++){
    //        thresholds[i] *= -1e-3;
    //    }


    // Get Rates:
    AidaTluControl myTlu;
    std::vector<std::vector<uint32_t>> rates(numThresholdValues, std::vector<uint32_t>(6));

    myTlu.DoStartUp();
    std::ofstream outFile;
    outFile.open ("output.txt");
    for (int i = 0; i < numThresholdValues; i++){
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

    //TH1F h("Vol")

    //    Gnuplot gp;

    //    gp << "set xrange [-2:2]\nset yrange [-2:2]\n";
    //            // '-' means read from stdin.  The send1d() function sends data to gnuplot's stdin.
    //            gp << "plot '-' with vectors title 'pts_A'\n";
    //            gp.send1d(pts_A);


    return 1;



    //    // for loop over threshold, save return of DoMeasureRate in dict
    //    for(int i = 0; i < numThresholdValues; i++){
    //        rates[thresholds[i]] = myTlu.DoMeasureRate(voltage, thresholds[i], time);
    //    }
}


/* I need:  - Init
            - Config
            - voltage setter
            - threshold setter
            - data collector
*/
