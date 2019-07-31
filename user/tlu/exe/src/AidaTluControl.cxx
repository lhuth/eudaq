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
#include <ctime>
#include <thread>
#include <map>
#include <math.h>

// ROOT includes
#include <TROOT.h>
#include <TGraph.h>
#include <TCanvas.h>
#include <TBrowser.h>
#include <TFrame.h>
#include <TFile.h>
#include <TApplication.h>
#include <TGaxis.h>

//#include "gnuplot-iostream.h"

class AidaTluControl {
public:
    AidaTluControl();
    void DoStartUp();
    void SetPMTVoltage(double voltage);
    void SetTLUThreshold(double threshold);
    void PlotMultiple(int numThresholdValues, int numTriggerInputs, std::string filename);
    std::vector<double> MeasureRate(int numTriggerInputs, std::vector<bool> connection, double voltage, double threshold, int time);

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
    m_tlu->SetI2C_expander2_addr(0);
    m_tlu->SetI2C_pwrmdl_addr(0x1C, 0x76, 0x77, 0x51);
    m_tlu->SetI2C_disp_addr(0x3A);

    // Initialize TLU hardware
    m_tlu->InitializeI2C(m_verbose);
    m_tlu->InitializeIOexp(m_verbose);
    // 1.3V = reference voltage
    m_tlu->InitializeDAC(false, 1.3, m_verbose);


    // Initialize the Si5345 clock chip using pre-generated file
    //    std::string defaultCfgFile= "file:///opt/eudaq2/user/eudet/misc/hw_conf/aida_tlu/fmctlu_clock_config.txt";
    //    int clkres;
    //    clkres= m_tlu->InitializeClkChip( defaultCfgFile, m_verbose  );
    //    if (clkres == -1){
    //        std::cout << "TLU: clock configuration failed." << std::endl;
    //    }

    // Set trigger stretch

    //    std::vector<unsigned int> stretcVec = {(unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10};
    //    m_tlu->SetPulseStretchPack(stretcVec, m_verbose);

    // Set trigger mask (high,low)
    // MIGHT BE ADJUSTED!!
    m_tlu->SetTriggerMask((uint32_t)0xFFFF,  (uint32_t)0xFFFE);

    // Reset IPBus registers
    m_tlu->ResetSerdes();
    m_tlu->ResetCounters();

    m_tlu->SetTriggerVeto(1, m_verbose); // no triggers
    m_tlu->ResetFIFO();
    m_tlu->ResetEventsBuffer();
    //m_tlu->ResetBoard();

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
std::vector<double> AidaTluControl::MeasureRate(int numTriggerInputs, std::vector<bool> connectionBool, double voltage, double threshold, int time){

    std::vector<uint32_t> sl={0,0,0,0,0,0};
    // Output: First for: TLU, last: Trigger Rate (pre & post veto)
    // Convert String input into bool input & count trigger inputs

    std::vector<double> output(numTriggerInputs + 2, 0);

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

    m_tlu->GetScaler(sl[0], sl[1], sl[2], sl[3], sl[4], sl[5]);
    output[numTriggerInputs - 2] = m_tlu->GetPreVetoTriggers();
    output[numTriggerInputs - 1] = m_tlu->GetPostVetoTriggers();


    std::cout << std::dec << sl[0] << "  " << sl[1]<< "  " << sl[2]<< "  " << sl[3]<< "  " << sl[4]<< "  " << sl[5] << std::endl;

    m_tlu->ResetCounters();
    m_tlu->ResetSerdes();



    m_duration = double(m_lasttime - m_starttime) / 1000000000; // in seconds
    std::cout << "Run duration [s]" << m_duration << std::endl;

    int i = 0;
    for (int k = 0; k < 6; k++){
        if (connectionBool[k]){
            output[i] = sl[k];
            i++;
        }
    }

    return {output};
}



void AidaTluControl::PlotMultiple(int numThresholdValues, int numTriggerInputs, std::string filename){


    std::ifstream infile;
    // TODO: Remove hard code
    std::string filename_ = "test.txt";
    infile.open(filename_);
    std::string line;
    int skiplines = 7;
    int noColumns = numTriggerInputs + 2; //+2 for Pro & PostVetoTrigger (not respecting threshold column)
    int lineCounter = 0;

    Double_t threshold[numThresholdValues];
    Double_t rate[noColumns][numThresholdValues];

    // read file
    if (infile.is_open())
    {
        while ( getline (infile,line) )
        {
            if (lineCounter >= skiplines){
                std::cout << line << std::endl;
                std::istringstream lineS;
                lineS.str(line);

                for (int i = 0; i < noColumns + 1; i++){
                    std::string val;
                    lineS >> val;
                    if (i == 0){
                        threshold[lineCounter - skiplines] = std::stod(val);
                        std::cout << threshold[lineCounter - skiplines] << '\n';
                    }
                    else{
                        rate[i-1][lineCounter - skiplines] = std::stod(val);
                        std::cout << rate[i-1][lineCounter - skiplines] << '\n';
                    }
                }
            }
            lineCounter++;
        }
        infile.close();
    }


    else std::cout << "Unable to open file";

    Double_t firstDer[noColumns][numThresholdValues];
    //Double_t thresholdFirstDer[numThresholdValues - 1] = threshold.pop_back();
    for (int i = 0; i < noColumns; i++){
        for (int j = 0; j < numThresholdValues - 1; j++){
            firstDer[i][j] =  rate[i][j] - rate[i][j+1];
        }
        firstDer[i][numThresholdValues-1] = firstDer[i][numThresholdValues-2];
    }

//    Double_t secDer[noColumns][numThresholdValues];
//    //Double_t thresholdsecDer[numThresholdValues - 1] = threshold.pop_back();
//    for (int i = 0; i < noColumns; i++){
//        for (int j = 0; j < numThresholdValues - 1; j++){
//            secDer[i][j] =  firstDer[i][j] - firstDer[i][j+1];
//        }
//        secDer[i][numThresholdValues-1] = secDer[i][numThresholdValues-2];
//    }


    Double_t secDer[noColumns][numThresholdValues];
    //Double_t thresholdsecDer[numThresholdValues - 1] = threshold.pop_back();
    for (int i = 0; i < noColumns; i++){
        for (int j = 1; j < numThresholdValues - 1; j++){
            secDer[i][j] =  rate[i][j+1] - 2.0*rate[i][j] + rate[i][j-1];
        }
        secDer[i][numThresholdValues-1] = secDer[i][numThresholdValues-2];
    }

    //    TGaxis::SetMaxDigits(2);
    TApplication *myApp = new TApplication("myApp", 0, 0);
    TCanvas *c1 = new TCanvas("c1", "Graph Draw Options", 200,10,1400,900);

    if (noColumns == 1) c1->Divide(1,1);
    else if (noColumns == 2) c1->Divide(2,1);
    else if (noColumns == 3) c1->Divide(3,1);
    else if (noColumns == 4) c1->Divide(2,2);
    else if (noColumns == 5) c1->Divide(3,2);
    else if (noColumns == 6) c1->Divide(3,2);
    else if (noColumns > 6) c1->Divide(3,3);
    else c1->Divide(5,3);

    TGraph *gr[noColumns];
    TGraph *gr2[noColumns];
    TGraph *gr3[noColumns];

    for (int i = 0; i < noColumns; i++){
        c1->cd(i+1);
   /*
        gr[i] = new TGraph (numThresholdValues,threshold,rate[i]);
        gr[i]->Draw("AL*");

        gr[i]->SetMarkerStyle(20);
        gr[i]->SetMarkerSize(0.5);
        gr[i]->SetMarkerColor(kRed + 2);
        std::string title = std::string("Channel ") + std::to_string(i+1) + std::string("; Threshold / V; Rate / Hz");
        gr[i]->SetTitle(title.c_str());
*/
      /*  gr2[i] = new TGraph (numThresholdValues,threshold,firstDer[i]);
        gr2[i]->Draw("AL*");*/

        gr3[i] = new TGraph (numThresholdValues,threshold,secDer[i]);
        gr3[i]->Draw("A*");

    }


    //    c1->SaveAs();

    c1->Update();
    c1->Modified();

    myApp->Run();


}


int main(int /*argc*/, char **argv) {
    eudaq::OptionParser op("EUDAQ Command Line FileReader modified for TLU", "2.1", "EUDAQ FileReader (TLU)");
    eudaq::Option<double> thrMin(op, "tl", "thresholdlow", -1.3, "double", "threshold value low [V]");
    eudaq::Option<double> thrMax(op, "th", "thresholdhigh", -0.1, "double", "threshold value high [V]");
    eudaq::Option<int> thrNum(op, "tn", "thresholdsteps", 9, "int", "number of threshold steps");
    eudaq::Option<double> volt(op, "v", "pmtvoltage", 0.9, "double", "PMT voltage [V]");
    eudaq::Option<int> acqtime(op, "t", "acquisitiontime", 10, "int", "acquisition time");
    eudaq::Option<std::string> name(op, "f", "filename", "output", "string", "filename");
    eudaq::Option<std::string> con(op, "c", "connectionmap", "110000", "string", "connection map");

    try{
        op.Parse(argv);
    }
    catch (...) {
        return op.HandleMainException();
    }


    // Threshold in [-1.3V,=1.3V] with 40e-6V presision
    double thresholdMin = thrMin.Value();
    double thresholdMax = thrMax.Value();
    double thresholdDifference = thresholdMax - thresholdMin;
    const int numThresholdValues = thrNum.Value();
    int time = acqtime.Value(); //time in seconds
    double voltage = volt.Value();
    const std::string filename = name.Value() + ".txt";
    if (filename == "output.txt") std::cout << "---------------CAUTION: FILENAME IS SET TO DEFAULT. DANGER OF DATA LOSS!---------------" <<std::endl;
    std::string connection = con.Value();

    int numTriggerInputs = 0;
    std::vector<bool> connectionBool(6, false);
    for (int i = 0; i < 6; i++){
        if(connection[i] == '1'){
            numTriggerInputs++;
            connectionBool[i] = true;
        }
    }

    // create array of thresholds
    double thresholds[numThresholdValues];

    if (numThresholdValues < 2) thresholds[0] = thresholdMin;
    else{
        for (int i = 0; i < numThresholdValues; i++){
            thresholds[i] = thresholdMin + i * thresholdDifference / (numThresholdValues-1);
        }
    }


    // Get Rates:
    AidaTluControl myTlu;
    std::vector<std::vector<double>> rates(numThresholdValues, std::vector<double>(numTriggerInputs + 2));

    std::ofstream outFile;
    outFile.open (filename);

    bool tluConnected = false;
    if(tluConnected){
        myTlu.DoStartUp();
        for (int i = 0; i < numThresholdValues; i++){
            rates[i] = myTlu.MeasureRate(numTriggerInputs, connectionBool, voltage, thresholds[i], time);
            //for (auto r:rates[i]) outFile << r << "   ";
            //outFile << "\n";

        }
    }

    else{
        // JUST FOR TEST
        for (int i = 0; i<9; i++){
            double j = i+1;
            //std::cout << rates[i][0]
            rates[i] = {exp(j),2*exp(j),3*exp(j),4*exp(j)};
        }
    }




    // write output File

    auto now = std::chrono::system_clock::now();
    std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    outFile << "Date:\t" << std::ctime(&timeNow) << "\n";
    outFile << "PMT Voltage [V]:\t   " << voltage << "\n";
    outFile << "Acquisition Time [s]\t:   " << time << "\n" << "\n";

    outFile << "Thr [V]\t\t";
    for (int i = 0; i < numTriggerInputs; i++){
        outFile << "PMT " << i+1 << " [Hz]\t";
    }
    outFile << "PreVeto [Hz]\t";
    outFile << "PostVeto [Hz]\t";
    outFile << "\n";


    for (int i = 0; i < numThresholdValues; i++){
        outFile << thresholds[i] << "\t\t";
        for (auto r:rates[i]) outFile << r << "\t\t";
        outFile << "\n";
    }

    outFile.close();

    myTlu.PlotMultiple(numThresholdValues, numTriggerInputs, filename);



    return 1;

}
