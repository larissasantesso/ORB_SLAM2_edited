/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/


#include<iostream>
#include<algorithm>
#include<fstream>
#include<iomanip>
#include<chrono>

#include<opencv2/core/core.hpp>

#include<System.h>

using namespace std;

void LoadImages(const string &strPathToSequence, vector<string> &vstrImageLeft,
                vector<string> &vstrImageRight, vector<double> &vTimestamps);

void LoadImagesWithMask(const string &strPathToSequence, vector<string> &vstrImageLeft, vector<string> &vstrImageRight, 
                        const string &strPathToSequence_mask, vector<string> &vstrMaskLeft, vector<string> &vstrMaskRight, 
                        vector<double> &vTimestamps);

int main(int argc, char **argv)
{
    if((argc < 4)||(argc > 6))
    {
        cerr << endl << "Usage: ./stereo_kitti path_to_vocabulary path_to_settings path_to_sequence kernel_size" << endl;
        return 1;
    }

    // Retrieve paths to images
    vector<string> vstrImageLeft;
    vector<string> vstrImageRight;
    vector<string> vstrMaskLeft;
    vector<string> vstrMaskRight;
    vector<double> vTimestamps;
    if(argc==4)
    {
        LoadImages(string(argv[3]), vstrImageLeft, vstrImageRight, vTimestamps);
    }
    else if(argc>=5)
    {   
        LoadImagesWithMask(string(argv[3]), vstrImageLeft, vstrImageRight, string(argv[4]), vstrMaskLeft, vstrMaskRight, vTimestamps);
    }
    const int nImages = vstrImageLeft.size();

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM2::System SLAM(argv[1],argv[2],ORB_SLAM2::System::STEREO,true);

    // Vector for tracking time statistics
    vector<float> vTimesTrack;
    vTimesTrack.resize(nImages);

    cout << endl << "-------" << endl;
    cout << "Start processing sequence ..." << endl;
    cout << "Images in the sequence: " << nImages << endl << endl;   

    // Main loop
    cv::Mat imLeft, imRight;
    cv::Mat maskLeftRaw, maskRightRaw;
    cv::Mat maskLeft, maskRight;
    cv::Mat kernel;

    
    if(argc>5)
    {
        kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(std::stoi(argv[5]),std::stoi(argv[5])));
    }

    for(int ni=0; ni<nImages; ni++)
    {
        // Read left and right images from file
        imLeft = cv::imread(vstrImageLeft[ni],CV_LOAD_IMAGE_UNCHANGED);
        imRight = cv::imread(vstrImageRight[ni],CV_LOAD_IMAGE_UNCHANGED);
        double tframe = vTimestamps[ni];

        if(imLeft.empty())
        {
            cerr << endl << "Failed to load image at: "
                 << string(vstrImageLeft[ni]) << endl;
            return 1;
        }
      
        if(!vstrMaskLeft.empty())
        {
            maskLeftRaw = cv::imread(vstrMaskLeft[ni],CV_LOAD_IMAGE_UNCHANGED);
            maskRightRaw = cv::imread(vstrMaskRight[ni],CV_LOAD_IMAGE_UNCHANGED);
            if(!kernel.empty())
            {
                // Right Mask
                erode(maskRightRaw, maskRight, kernel);

                // Left Mask
                // Since the masks need to be black because of the filter applied in the keyframes
                // The erosion here will dilate the black masks.  
                erode(maskLeftRaw, maskLeft, kernel);
            }
            else
            {
                maskRight = maskRightRaw;
                maskLeft = maskLeftRaw;
            }
        }

#ifdef COMPILEDWITHC11
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
#else
        std::chrono::monotonic_clock::time_point t1 = std::chrono::monotonic_clock::now();
#endif
        // Pass the images to the SLAM system
        if(maskLeft.empty())
            SLAM.TrackStereo(imLeft,imRight,tframe);
        
        else if(!maskLeft.empty())
            SLAM.TrackStereoWithMask(imLeft,imRight,maskLeft,maskRight,tframe);

#ifdef COMPILEDWITHC11
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
#else
        std::chrono::monotonic_clock::time_point t2 = std::chrono::monotonic_clock::now();
#endif

        double ttrack= std::chrono::duration_cast<std::chrono::duration<double> >(t2 - t1).count();

        vTimesTrack[ni]=ttrack;

        // Wait to load the next frame
        double T=0;
        if(ni<nImages-1)
            T = vTimestamps[ni+1]-tframe;
        else if(ni>0)
            T = tframe-vTimestamps[ni-1];

        if(ttrack<T)
            usleep((T-ttrack)*1e6);
    }

    // Stop all threads
    SLAM.Shutdown();

    // Tracking time statistics
    sort(vTimesTrack.begin(),vTimesTrack.end());
    float totaltime = 0;
    for(int ni=0; ni<nImages; ni++)
    {
        totaltime+=vTimesTrack[ni];
    }
    cout << "-------" << endl << endl;
    cout << "median tracking time: " << vTimesTrack[nImages/2] << endl;
    cout << "mean tracking time: " << totaltime/nImages << endl;

    // Save camera trajectory
    SLAM.SaveTrajectoryKITTI("CameraTrajectory.txt");

    return 0;
}

void LoadImages(const string &strPathToSequence, vector<string> &vstrImageLeft,
                vector<string> &vstrImageRight, vector<double> &vTimestamps)
{
    ifstream fTimes;
    string strPathTimeFile = strPathToSequence + "/times.txt";
    string strPathFrameName = strPathToSequence + "/frames.txt";
    fTimes.open(strPathTimeFile.c_str());
    while(!fTimes.eof())
    {
        string s;
        getline(fTimes,s);
        if(!s.empty())
        {
            stringstream ss;
            ss << s;
            double t;
            ss >> t;
            vTimestamps.push_back(t);
        }
    }

    string strPrefixLeft = strPathToSequence + "/image_resized_0/";
    string strPrefixRight = strPathToSequence + "/image_resized_1/";

    const int nTimes = vTimestamps.size();
    vstrImageLeft.resize(nTimes);
    vstrImageRight.resize(nTimes);
    
    ifstream fFrames;
    int i=0;
    fFrames.open(strPathFrameName.c_str());
    while(!fFrames.eof())
    {
        string ss;
        
        getline(fFrames, ss);
        if(!ss.empty())
        {
        vstrImageLeft[i] = strPrefixLeft + ss + "_leftImg8bit.png";
        vstrImageRight[i] = strPrefixRight + ss + "_rightImg8bit.png";
        i++;
        }
    }
}

void LoadImagesWithMask(const string &strPathToSequence, vector<string> &vstrImageLeft,
                vector<string> &vstrImageRight,  const string &strPathToSequence_mask, 
                vector<string> &vstrMaskLeft, vector<string> &vstrMaskRight, vector<double> &vTimestamps)
{
    ifstream fTimes;
    string strPathTimeFile = strPathToSequence + "/times.txt";
    string strPathFrameName = strPathToSequence + "/frames.txt";
    fTimes.open(strPathTimeFile.c_str());
    while(!fTimes.eof())
    {
        string s;
        getline(fTimes,s);
        if(!s.empty())
        {
            stringstream ss;
            ss << s;
            double t;
            ss >> t;
            vTimestamps.push_back(t);
        }
    }

    string strPrefixLeft = strPathToSequence + "/image_resized_0/";
    string strPrefixRight = strPathToSequence + "/image_resized_1/";
  
    string strPrefixLeft_mask = strPathToSequence_mask + "/image_resized_0/";
    string strPrefixRight_mask = strPathToSequence_mask + "/image_resized_1/";

    const int nTimes = vTimestamps.size();
    vstrImageLeft.resize(nTimes);
    vstrImageRight.resize(nTimes);
    vstrMaskLeft.resize(nTimes);
    vstrMaskRight.resize(nTimes);
    
    ifstream fFrames;
    int i=0;
    fFrames.open(strPathFrameName.c_str());
    while(!fFrames.eof())
    {
        string ss;
        
        getline(fFrames, ss);
        if(!ss.empty())
        {
        vstrImageLeft[i] = strPrefixLeft + ss + "_leftImg8bit.png";
        vstrImageRight[i] = strPrefixRight + ss + "_rightImg8bit.png";
        vstrMaskLeft[i] = strPrefixLeft_mask + ss + "_leftImg8bit.png";
        vstrMaskRight[i] = strPrefixRight_mask + ss + "_rightImg8bit.png";
        i++;
        }
    }
}
  
