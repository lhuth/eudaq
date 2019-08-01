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
#include <TF1.h>
#include <TLatex.h>
#include <TLegend.h>


//#include "gnuplot-iostream.h"

class AidaTluControl {
public:
    AidaTluControl();
    void DoStartUp();
    void SetPMTVoltage(double voltage);
    void SetTLUThreshold(double threshold);
    std::vector<double> GetOptimalThreshold(int numThresholdValues, int numTriggerInputs, std::string filename);
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


std::vector<double> AidaTluControl::GetOptimalThreshold(int numThresholdValues, int numTriggerInputs, std::string filename){

    // Open File with data and write them into arrays
    std::ifstream infile;
    // TODO: Remove hard code
    std::string filename_ = "test_few.txt";
    infile.open(filename_);
    std::string line;
    int skiplines = 2;
    int lineCounter = 0;

    Double_t threshold[numThresholdValues];
    Double_t rate[numTriggerInputs][numThresholdValues];

    // read file
    if (infile.is_open())
    {
        while ( getline (infile,line) )
        {
            if (lineCounter >= skiplines){
                std::istringstream lineS;
                lineS.str(line);

                for (int i = 0; i < numTriggerInputs + 1; i++){
                    std::string val;
                    lineS >> val;
                    if (i == 0){
                        threshold[lineCounter - skiplines] = std::stod(val);
                    }
                    else{
                        rate[i-1][lineCounter - skiplines] = std::stod(val);
                    }
                }
            }
            lineCounter++;
        }
        infile.close();
    }
    else std::cout << "Unable to open file";

    // Calculate linearized derivative of rate
    Double_t derivative[numTriggerInputs][numThresholdValues];
    for (int i = 0; i < numTriggerInputs; i++){
        for (int j = 0; j < numThresholdValues - 1; j++){
            derivative[i][j] =  rate[i][j] - rate[i][j+1];
        }
        derivative[i][numThresholdValues-1] = derivative[i][numThresholdValues-2]; //extrapolate last point
    }

    //Plot Data and determine optimum

    std::vector<double> optimalThreshold(numTriggerInputs);
    TApplication *myApp = new TApplication("myApp", 0, 0);
    TCanvas *c1 = new TCanvas("c1", "Graph Draw Options", 0,10,940,1200);
    TCanvas *c2 = new TCanvas("c2", "Graph Draw Options", 980,10,940,1200);

    if (numTriggerInputs == 1) c1->Divide(1,1);
    else if (numTriggerInputs == 2) c1->Divide(2,1);
    else if (numTriggerInputs == 3) c1->Divide(3,1);
    else if (numTriggerInputs == 4) c1->Divide(2,2);
    else if (numTriggerInputs == 5) c1->Divide(3,2);
    else if (numTriggerInputs == 6) c1->Divide(3,2);
    else if (numTriggerInputs > 6) c1->Divide(3,3);
    else c1->Divide(5,3);

    if (numTriggerInputs == 1) c2->Divide(1,1);
    else if (numTriggerInputs == 2) c2->Divide(2,1);
    else if (numTriggerInputs == 3) c2->Divide(3,1);
    else if (numTriggerInputs == 4) c2->Divide(2,2);
    else if (numTriggerInputs == 5) c2->Divide(3,2);
    else if (numTriggerInputs == 6) c2->Divide(3,2);
    else if (numTriggerInputs > 6) c2->Divide(3,3);
    else c2->Divide(5,3);

    TGraph *gr[numTriggerInputs];
    TGraph *gr2[numTriggerInputs];
    TGraph *grPlateau[numTriggerInputs];
    TGraph *grMidpoint[numTriggerInputs];

    for (int i = 0; i < numTriggerInputs; i++){
        c1->cd(i+1);

        gr[i] = new TGraph (numThresholdValues,threshold,rate[i]);
        gr[i]->Draw("AL*");

        gr[i]->SetMarkerStyle(20);
        gr[i]->SetMarkerSize(0.5);
        gr[i]->SetMarkerColor(kBlue + 1);
        std::string title = std::string("TLU ") + std::to_string(i+1) + std::string("; Threshold / V; Rate / Hz");
        gr[i]->SetTitle(title.c_str());

        c2->cd(i+1);
        gr2[i] = new TGraph (numThresholdValues,threshold,derivative[i]);

        // Fit Gaussian to first derivative
        TF1 *f = new TF1("f", "gaus", -1.3, -0.2);
        gr2[i]->Fit(f, "RQ"); //R: fit only in predefined range Q:Quiet
        double meanGaus = f->GetParameter(1);
        double constGaus = f->GetParameter(0);
        gr2[i]->Draw("AL*");
        gr2[i]->SetMarkerStyle(20);
        gr2[i]->SetMarkerSize(0.5);
        gr2[i]->SetMarkerColor(kBlue + 1);
        std::string titleDerivative = std::string("TLU ") + std::to_string(i+1) + std::string("; Threshold / V; a.U.");
        gr[i]->SetTitle(titleDerivative.c_str());

        // Find Plateau
        grPlateau[i] = new TGraph();
        double coefficient = 0.2;

        //Make sure at least one Plateau point is found, otherwise: lower condition
        while ((grPlateau[i]->GetN() == 0) & (coefficient <= 1)){
            int k = 0;
            for (int j = 0; j < numThresholdValues; j++){
                if ((threshold[j] > meanGaus) & (derivative[i][j] <= coefficient * constGaus)){
                    grPlateau[i]->SetPoint(k, threshold[j], rate[i][j]);
                    k++;
                }

            }
            coefficient +=0.05;
        }

        // Find Midpoint of Plateau
        grMidpoint[i] = new TGraph();
        int lenPlateau = grPlateau[i]->GetN();
        int indexMidpoint = (lenPlateau + 1*lenPlateau%2)/2 - 1;
        grMidpoint[i]->SetPoint(0, grPlateau[i]->GetX()[indexMidpoint], grPlateau[i]->GetY()[indexMidpoint]);

        // Plot Plateau and Midpoint
        c1->cd(i+1);
        grPlateau[i]->Draw("*");
        grPlateau[i]->SetMarkerStyle(20);
        grPlateau[i]->SetMarkerSize(1);
        grPlateau[i]->SetMarkerColor(kRed + 1);
        grMidpoint[i]->Draw("*");
        grMidpoint[i]->SetMarkerStyle(20);
        grMidpoint[i]->SetMarkerSize(1);
        grMidpoint[i]->SetMarkerColor(kGreen + 1);
        grMidpoint[i]->SetTitle("Optimal Threshold");


        //Plot legend
        auto legend = new TLegend(0.1,0.8,0.58,0.9);
        std::string midPointString = std::to_string(grMidpoint[i]->GetX()[0]);
        midPointString.erase(midPointString.begin()+6, midPointString.end()); //limit number of values after comma
        std::string labelMidpoint = std::string("Optimal Threshold:  ") + midPointString + std::string(" V");
        legend->AddEntry(grMidpoint[i],labelMidpoint.c_str(),"p");
        legend->Draw();

        optimalThreshold[i] = grMidpoint[i]->GetX()[0];
    }

    c1->Update();
    c1->Modified();
    c2->Update();
    c2->Modified();
    std::cout << "Enter something to continue" << std::endl;
    int k;
    std::cin >> k;

    //myApp->Run();
    //myApp->Terminate();


    return optimalThreshold;
}



int main(int /*argc*/, char **argv) {
    eudaq::OptionParser op("EUDAQ Command Line FileReader modified for TLU", "2.1", "EUDAQ FileReader (TLU)");
    eudaq::Option<double> thrMin(op, "tl", "thresholdlow", -1.3, "double", "threshold value low [V]");
    eudaq::Option<double> thrMax(op, "th", "thresholdhigh", -0.1, "double", "threshold value high [V]");
    eudaq::Option<int> thrNum(op, "tn", "thresholdsteps", 9, "int", "number of threshold steps");
    eudaq::Option<double> volt(op, "v", "pmtvoltage", 0.9, "double", "PMT voltage [V]");
    eudaq::Option<int> acqtime(op, "t", "acquisitiontime", 10, "int", "acquisition time");
    eudaq::Option<std::string> name(op, "f", "filename", "output", "string", "filename");
    eudaq::Option<std::string> con(op, "c", "connectionmap", "111100", "string", "connection map");

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

    std::vector<double> optimalThresholds;
    optimalThresholds = myTlu.GetOptimalThreshold(numThresholdValues, numTriggerInputs, filename);

    std::cout << "___________________" << std::endl;
    std::cout << "Optimal Thresholds" << std::endl;
    for (int i = 0; i < numTriggerInputs; i++){
        std::cout << "TLU " << i+1 << ":   " << optimalThresholds[i]<< " V" << std::endl;
    }
    std::cout << "___________________" << std::endl;


    return 1;

}
