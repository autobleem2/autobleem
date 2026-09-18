//
// Created by screemer on 23.03.2020.
//

#include "bluetool.h"
#include "../util.h"

string specialCommands[] = {"[NEW]","[CHG]","[DEL]"};



string bluetool::clearColors(string input)
{
    string colorCoding[]={"\x1B[0m","\x1B[0;91m","\x1B[0;92m","\x1B[0;93m","\x1B[0;94m","\x1B[1;30m", "\x1B[1;37m" "\x1B[1;39m"};
    for (string colorCode:colorCoding)
    {
        if (input.find(colorCode)!=string::npos)
        {
            std::string::size_type i = input.find(colorCode);
            while (i != std::string::npos) {
                input.erase(i, colorCode.length());
                i = input.find(colorCode, i);
            }
        }
    }
    string filtered = "";
    for (int i=0;i<input.size();i++)
    {
        if ((input[i]> 31) && (input[i]<127)) filtered+=input[i];
    }
    return Util::trim(filtered);
}

void bluetool::splitLines(char *buffer)
{
    string line = "";
    for (int i=0;i<BUFF_SIZE;i++)
    {
        if (buffer[i]=='\n')
        {
            receivedLines.push(line);
            line="";
        } else {
            if (buffer[i] == 0) {
               // cout << "REST" << line << endl;
                receivedLines.push(line);
                break;
            }
            line+=buffer[i];
        }
    }
}
void bluetool::init() {
    /*
   // sockpp::socket_initializer sockInit;
    connectionOk = false;

  //  in_port_t port = this->port;

   // conn = new sockpp::tcp_connector({address, port});
    if (!&conn) {
        cerr << "Error connecting to server at "
             << sockpp::inet_address(address, port)
             << "\n\t" << conn->last_error_str() << endl;
        return;
    }

    if (!conn->set_non_blocking(true)) {
        cerr << "Error creating nonblocking socket: "
             << conn->last_error_str() << endl;
        return;
    }

    connectionOk = true;
     */
}

int msgNumber=1;
void bluetool::processSocket() {
    /*
    if (!connectionOk) {
        return;
    }
    memset(buffer,1,BUFF_SIZE);
    ssize_t n = conn->read_n(buffer, BUFF_SIZE);

    if (n != -1) {
        splitLines(buffer);
        while (!receivedLines.empty())
        {
            string line = receivedLines.front();
            line = clearColors(line);
            receivedLines.pop();
            cout << msgNumber << line << endl;
            msgNumber++;
        }

    } else {
        if (!cmdQueue.empty()) {
            bt_comamnd cmd = cmdQueue.back();
            conn->write(cmd.command + "\n");

        } else {

        }
    }
     */
}

void bluetool::quit() {
    /*
    if (conn->is_open()) {
        conn->close();
    }
    delete conn;
     */
}