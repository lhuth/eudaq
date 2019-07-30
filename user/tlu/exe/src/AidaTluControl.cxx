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
    void PlotData(Int_t numThresholdValues, Double_t *threshold, Double_t *rate);
    //void PlotMultiple(Int_t numThresholdValues, Double_t *threshold1, Double_t *rate1, Double_t *threshold2, Double_t *rate2);
    void PlotMultiple(std::string filename);
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
    m_tlu->SetI2C_expander2_addr(0);
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

    //    std::vector<unsigned int> stretcVec = {(unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10,
    //                                           (unsigned int) 10};
    //    m_tlu->SetPulseStretchPack(stretcVec, m_verbose);


    // Reset IPBus registers
    m_tlu->ResetSerdes();
    m_tlu->ResetCounters();

    m_tlu->SetTriggerVeto(1, m_verbose); // no triggers
    m_tlu->ResetFIFO();
    m_tlu->ResetEventsBuffer();
    m_tlu->ResetBoard();

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



//void AidaTluControl::PlotMultiple(Int_t numThresholdValues, Double_t *threshold1, Double_t *rate1, Double_t *threshold2, Double_t *rate2){
//    TApplication *myApp = new TApplication("myApp", 0, 0);
//    TCanvas *c1 = new TCanvas("c1", "Graph Draw Options", 200,10,1400,700);
//    c1->Divide(2,1);

//    c1->cd(1);
//    TGraph *gr1 = new TGraph (numThresholdValues,threshold1,rate1);

//    gr1->Draw("A*");
//    gr1->SetMarkerStyle(20);
//    gr1->SetMarkerSize(1);
//    gr1->SetMarkerColor(kRed + 1);
//    gr1->SetTitle("Channel 1; Threshold / V; Rate / Hz");


//    c1->cd(2);
//    TGraph *gr2 = new TGraph (numThresholdValues,threshold2,rate2);

//    gr2->Draw("A*");
//    gr2->SetMarkerStyle(20);
//    gr2->SetMarkerSize(1);
//    gr2->SetMarkerColor(kRed + 1);
//    gr2->SetTitle("Channel 2; Threshold / V; Rate / Hz");

//    c1->Update();
////    c1->GetFrame()->SetBorderSize(120);
//    c1->Modified();

//    myApp->Run();
//}


std::vector<std::vector<uint32_t>> rates(10, std::vector<uint32_t>(6));
std::string outString;

/*
int channelNo = 0;
while(std::getline(infile,outString)){
    std::istringstream csvStream(outString);
    //std::cout << outString << std::endl;
    std::string outElement;
    int thresholdNo = 0;
    while (std::getline(csvStream, outElement, ',')){

        rates[thresholdNo][channelNo] = std::stoi(outElement);
        std::cout << thresholdNo << channelNo << std::endl;
        std::cout << rates[0][thresholdNo] << std::endl;
        std::cout<<"______________________" << std::endl;
        thresholdNo += 1;



    }
    channelNo += 1;
}
for (int thresholdNo = 0; thresholdNo <10; thresholdNo++){
    std::cout << rates[0][thresholdNo] << std::endl;
}
infile.close();*/

//istringstream iss;

//string value = "32 40 50 80 902";

//    iss.str (value); //what does this do???
//    for (int i = 0; i < 5; i++) {
//    string val;
//        iss >> val;
//    cout << i << endl << val <<endl;
//    }

//    return 0;
//}


void AidaTluControl::PlotMultiple(std::string filename){
    std::ifstream infile;
    infile.open(filename);
    std::string line;
    int skiplines = 6;
    int noColumns = 7;
    int lineCounter = 0;
    int numThresholdValues = 9;

    Double_t threshold[9]; //TODO: Remove hot fix!!
    Double_t rate[noColumns-1][9];

    // read file
    if (infile.is_open())
    {
        while ( getline (infile,line) )
        {
            if (lineCounter >= skiplines){
                std::cout << line << std::endl;
                std::istringstream lineS;
                lineS.str(line);

                for (int i = 0; i < noColumns; i++){
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



                //std::string outElement;
//                while (std::getline((std::istringstream)line, outElement, '\t')){
//                    std::cout << outElement << '\n';
//                }


            }
            lineCounter++;

        }

        infile.close();
    }




    else std::cout << "Unable to open file";



//    TGaxis::SetMaxDigits(2);
    TApplication *myApp = new TApplication("myApp", 0, 0);
    TCanvas *c1 = new TCanvas("c1", "Graph Draw Options", 200,10,1400,700);
    c1->Divide(3,2);


    c1->cd(1);
    TGraph *gr1 = new TGraph (numThresholdValues,threshold,rate[0]);
    gr1->Draw("A*");
    gr1->SetTitle("Channel 1; Threshold / V; Rate / Hz");

    c1->cd(2);
    TGraph *gr2 = new TGraph (numThresholdValues,threshold,rate[1]);
    gr2->Draw("A*");
    gr2->SetTitle("Channel 2; Threshold / V; Rate / Hz");

    c1->cd(3);
    TGraph *gr3 = new TGraph (numThresholdValues,threshold,rate[2]);
    gr3->Draw("A*");
    gr3->SetTitle("Channel 3; Threshold / V; Rate / Hz");

    c1->cd(4);
    TGraph *gr4 = new TGraph (numThresholdValues,threshold,rate[3]);
    gr4->Draw("A*");
    gr4->SetTitle("Channel 4; Threshold / V; Rate / Hz");

    c1->cd(5);
    TGraph *gr5 = new TGraph (numThresholdValues,threshold,rate[4]);
    gr5->Draw("A*");
    gr5->SetTitle("Channel 5; Threshold / V; Rate / Hz");

    c1->cd(6);
    TGraph *gr6 = new TGraph (numThresholdValues,threshold,rate[5]);
    gr6->Draw("A*");
    gr6->SetTitle("Channel 6; Threshold / V; Rate / Hz");




//    c1->SaveAs();

    /*
    gr1->SetMarkerStyle(20);
    gr1->SetMarkerSize(1);
    gr1->SetMarkerColor(kRed + 1);



    c1->cd(2);
    TGraph *gr2 = new TGraph (numThresholdValues,threshold2,rate2);

    gr2->Draw("A*");
    gr2->SetMarkerStyle(20);
    gr2->SetMarkerSize(1);
    gr2->SetMarkerColor(kRed + 1);
    gr2->SetTitle("Channel 2; Threshold / V; Rate / Hz");

    */
    c1->Update();
    //    c1->GetFrame()->SetBorderSize(120);
    c1->Modified();

    myApp->Run();


}
void AidaTluControl::PlotData(Int_t numThresholdValues, Double_t *threshold, Double_t *rate){


    TGraph *gr1 = new TGraph (numThresholdValues,threshold,rate);

    gr1->Draw("A*");
    gr1->SetMarkerStyle(20);
    gr1->SetMarkerSize(1);
    gr1->SetMarkerColor(kRed + 1);

}

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
    eudaq::Option<std::string> name(op, "f", "filename", "output", "string", "filename");

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
    std::cout << filename <<std::endl;

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
    std::vector<std::vector<double>> rates(numThresholdValues, std::vector<double>(6));
    //std::vector<std::vector<uint32_t>> rates;

    std::cout << rates.size() << "   " << rates[0].size() << std::endl;
    for (int i = 0; i<9; i++){
        double j = i+1;
        //std::cout << rates[i][0]
        rates[i] = {exp(j),2*exp(j),3*exp(j),4*exp(j),5*exp(j),6*exp(j)};
    }

    std::cout << "fine" << std::endl;

    //myTlu.DoStartUp();
    std::ofstream outFile;
    outFile.open (filename);
    /*for (int i = 0; i < numThresholdValues; i++){
        rates[i] = myTlu.MeasureRate(voltage, thresholds[i], time);
        std::cout << "Threshold: " << thresholds[i]*1e3 << "mV" << std::endl;
        //std::cout << "Counts:";
        //for (auto r:rates) std::cout << r << "   ";
        //std::cout << std::endl;
        //std::cout <<rates[i][0] << std::endl;
        std::cout << "_______________________" << std::endl;

        for (auto r:rates[i]) outFile << r << "   ";
        outFile << "\n";

    }*/

    // write output File

    auto now = std::chrono::system_clock::now();
    std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
    outFile << "Date:\t" << std::ctime(&timeNow) << "\n";
    outFile << "PMT Voltage [V]:\t   " << voltage << "\n";
    outFile << "Acquisition Time [s]\t:   " << time << "\n" << "\n";

    outFile << "Thr [V]\t\t";
    for (int i = 0; i < 6; i++){
        outFile << "PMT " << i+1 << " [Hz]\t";
    }
    outFile << "\n";


    for (int i = 0; i < numThresholdValues; i++){
        outFile << thresholds[i] << "\t";
        for (auto r:rates[i]) outFile << r << "\t";
        outFile << "\n";
    }

    outFile.close();

    myTlu.PlotMultiple(filename);
    /*
    Int_t n = 9;
    //Double_t x[n] = {4,5,6,7,8,9,10,20,30,40};
    //Double_t y[n] = {40294,2879,26,120,29,9,8,60,8,8};

    Double_t x[n] = {0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0};
    Int_t t = 30; //time in s
    Double_t y[n] = {100000, 10000, 1000, 100, 10, 5, 4, 3 ,3 ,2};


    for (Int_t i = 0; i < n; i++){
        y[i] /= t; //transfer no of counts into rate
    }

    Double_t y2[n];
    for (Int_t i = 0; i < n; i++){
        y2[i] = y[i] /2; //transfer no of counts into rate
    }
    */
    /*
    myTlu.PlotData(n, x, y);


    myTlu.PlotData(n, x, y2);
    */
    //myTlu.PlotMultiple(n, x, y, x, y2);

    //TH1F h("Vol")

    //    Gnuplot gp;

    //    gp << "set xrange [-2:2]\nset yrange [-2:2]\n";
    //            // '-' means read from stdin.  The send1d() function sends data to gnuplot's stdin.
    //            gp << "plot '-' with vectors title 'pts_A'\n";
    //            gp.send1d(pts_A);


    return 1;

}
