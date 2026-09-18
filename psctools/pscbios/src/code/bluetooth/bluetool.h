//
// Created by screemer on 23.03.2020.
//

#pragma once

#include <string>
#include <vector>
#include <map>
#include <queue>
//#include <sockpp/tcp_connector.h>
#include <cstdio>

using namespace std;


#define DEVICE_TYPE_CONTROLLER 0
#define DEVICE_TYPE_DEVICE     1

#define EVENT_NEW    0
#define EVENT_DELETE 1
#define EVENT_CHANGE 2




class bt_comamnd {
public:
    bt_comamnd(string cmd)
    {
        command = cmd;
        response.clear();
    }
    string command;
    vector<string> response;
    // callback
};

class bt_deviceData {
public:
    int type;
    string address;
    string name;
    string alias;
    string deviceClass;
    string icon;
    bool pairable = false;
    bool powered = false;
    bool discovering = false;
    bool pairing = false;
    bool paired = false;
    bool trusted = false;
    bool blocked = false;
    bool connected = false;
    bool legacyPairing = false;
    string modalias;
    vector<string> uuid;
    long pairableTimeout;

    bool Trusting = false;
};

#define BUFF_SIZE  1024*50

class bluetool {
private:


    string clearColors(string input);
public:
    char buffer[BUFF_SIZE];
    bool connectionOk = false;
    queue<string> receivedLines;
    queue<string> controlLines;

    //sockpp::tcp_connector *conn;

    map<string, bt_deviceData> devices;
    queue<bt_comamnd> cmdQueue;

    string address="192.168.2.85";
    int port=787;
    bool canRunCommand=false;
    bool canRunCommands=false;

    void readCommandAnswer(bt_comamnd command)
    {

    }
    void runCommand(string command)
    {
        if (canRunCommand && canRunCommands) {
            canRunCommand = false;
        }
        bt_comamnd cmd(command);
      //  conn->write(command+"\n");
        readCommandAnswer(command);
    }

    void setPower(bool state) { runCommand("power " + string(state ? "on" : "off") +"\n"); };

    void setPairable(bool state) { runCommand("pairable " + string(state ? "on" : "off") +"\n"); };

    void setDiscoverable(bool state) { runCommand("discoverable " + string(state ? "on" : "off") +"\n"); };

    void setScan(bool state) { runCommand("scan " + string(state ? "on" : "off") +"\n"); };

    void trustDevice(string deviceAddress) { runCommand("trust " + deviceAddress +"\n"); };

    void untrustDevice(string deviceAddress) { runCommand("untrust " + deviceAddress +"\n"); };

    void blockDevice(string deviceAddress) { runCommand("block " + deviceAddress +"\n"); };

    void unblockDevice(string deviceAddress) { runCommand("unblock " + deviceAddress +"\n"); };

    void pairDevice(string deviceAddress) { runCommand("pair " + deviceAddress +"\n"); };

    void removeDevice(string deviceAddress) { runCommand("remove " + deviceAddress +"\n"); };

    void connectDevice(string deviceAddress) { runCommand("connect " + deviceAddress +"\n"); };

    void disconnectDevice(string deviceAddress) { runCommand("disconnect " + deviceAddress +"\n"); };

    void removeAllDevices()
    {

    }

    void init();
    void processSocket();
    void quit();

    void splitLines(char * buffer);

};




