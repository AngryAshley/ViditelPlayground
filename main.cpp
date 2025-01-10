#include <iostream>
#include "include/serial.h"
#include "boost/exception/all.hpp"
#include <chrono>
#include <thread>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <clocale>



#include <sys/select.h>
bool termAvailable(){
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, nullptr, nullptr, &tv);
    return(FD_ISSET(0,&fds));
}

static const constexpr auto term_ok =   "[ \e[1;32mOK\e[0m ]";
static const constexpr auto term_warn = "[\e[1;33mWARN\e[0m]";
static const constexpr auto term_fail = "[\e[1;31mFAIL\e[0m]";
static const constexpr auto term_info = "[\e[37mINFO\e[0m]";
static const constexpr auto term_debug = "[\e[1;36mDBUG\e[0m]";
static const constexpr auto term_def =  "[\e[37m....\e[0m]";
static const constexpr auto term_modemData =  "[\e[37mMODD\e[0m]";
static const constexpr auto term_modemComm =  "[\e[35mMODC\e[0m]";
static const constexpr auto term_empty = "      "; //for commenting on result from previous line?
static char waitingSymbol[4] = {'|','/','-','\\'};

std::string_view path_exe; 

void termFillPrevDef(auto term_prefix, int linesAboveCursor = 1){
    std::cout << std::format("\e[s\r\e[{1:}A{0:}\e[u",term_prefix,linesAboveCursor); //save cursor pos, move a safe amount to left, move linesAboveCursor up and print the prefix
}

serial::Serial s;
std::string lastModemError;

int modemCommand(const char* const command, bool verbose = false){
    std::string commandString = command;
    commandString+="\r\n";

    if(verbose)std::cout << std::format("{0:} Modem command: {1:}",term_debug, command) << std::flush;
    s.transmit({commandString.c_str()});

    auto future = s.receiveAsync(40, 5000, "OK\r\n");
        
        
        
        if(verbose)while(future.wait_for(std::chrono::milliseconds(500)) != std::future_status::ready)
            std::cout << '.' <<std::flush;


	const std::vector<uint8_t> received_data = future.get();

	if (received_data.size() < 1){
        std::cout << std::format("{0:} Modem did not respond.\n",term_fail);
        return 1;
    } else {
		
    std::string_view cleanData {reinterpret_cast<const char * const>(received_data.data()), received_data.size()};
    std::string verboseReply = commandString + "\r\nOK\r\n";
    if(cleanData=="\r\nOK\r\n"||cleanData == verboseReply||cleanData=="\r\n"){ //silent w/ modem response, verbose and silent wo/ modem response
            if(verbose)std::cout <<", reply OK\n";
            return 0;
        } else {
            std::string modemError;
            char modemErrorChar[6];
            for (auto val : cleanData) {
                    val>19?sprintf(modemErrorChar,"%c",val):sprintf(modemErrorChar,"\\x%02X",val);
                    modemError += modemErrorChar;
		    }
            lastModemError=modemError;
            if(verbose){
                printf(", reply: ");
                for (auto val : cleanData) {
                    printf("%02X ",val);
		        }
                printf("(%s)\n",modemError.c_str());
            }
            return 1;
        }
    }
}

int modemHangup(){
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    s.transmit("+");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto end = std::chrono::high_resolution_clock::now();
    s.transmit("+");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    s.transmit("+");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    modemCommand("ATH\r\n");
    return 1;
}

int modemReset(){
    if(!modemCommand("ATZ")){
        std::cout << std::format("{0:} Modem online and successfully reset\n",term_info,2);
        std::cout << std::format("{0:} Initializing...\n",term_def);
    }
    
    sleep(1); // MAKE THIS NEAT AND FIGURE OUT HOW LONG TO WAIT!

    bool initFailed = false;
    if(!modemCommand("ATE0")){//disable echo
    } else {
        termFillPrevDef(term_warn);
        std::cout << std::format("{0:} Modem Command ATE0 responded in unexpected way: {1:}\n",term_empty,lastModemError);
        initFailed=true;
    }
    if(!modemCommand("ATQ0")){ //enable command responses
     } else {
        termFillPrevDef(term_warn);
        std::cout << std::format("{0:} Modem Command ATQ0 responded in unexpected way: {1:}\n",term_empty,lastModemError);
        initFailed=true;
    }
    if(!modemCommand("ATB3")){ //set communication standard to CCITT V.23 main(answering) channel 
     } else {
        termFillPrevDef(term_warn);
        std::cout << std::format("{0:} Modem Command ATB3 responded in unexpected way: {1:}\n",term_empty,lastModemError);
        initFailed=true;
    }
    if(!initFailed){
        termFillPrevDef(term_ok);
        std::cout << std::format("{0:} Modem initialised successfully.\n",term_def);
    } else {
        termFillPrevDef(term_fail);
        std::cout << std::format("{0:} Modem initialisation failed. See above errors.\n\n",term_empty);
        return -1;
    }
    return 0;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SERVICE SECTION
//
// \x0c clear screen
//
// x08 is left x09 is right 0A is down 0b is up
// x1b doet iets? x1e cursor home?
//
// 14 hide cursor? 11 show cursor
// vergeet niet receiveasync om met de daadwerkelijk de juiste wacht tijd te kiezen.
// 12 repeat char?
// 18 is wipeout?  jaa misschien ja ja heel waarscjhoknlijk

// 1b 1c = corrupts characters and mentions videotex???? what happened here? occurns on start page when 0x1d interception in tx2 mode is reenabled for tex mode.

// 1b 21 = inverted a?
// 1b 3b = ! and special chars inverted
// 1b 3f = hollow block character?


// 1b 41 red for that line
// 1b 42 green
// 1b 43 yellow
// 1b 44 blue
// 1b 45 magenta
// 1b 46 cyan
// 1b 47 wit
// 1b 48 blinking
// 1b 49 blink off
// 1b 4c normal height
// 1b 4d double height
// 1b 51 red moasic
// 1b 52 green mosaic
// 1b 53 yellow mosaic
// 1b 54 blue mosaic
// 1b 55 magenta mosaic
// 1b 56 cyan mosaic
// 1b 57 white mosaic
// 1b 58 hide?
// 1b 59 continuous mosaic
// 1b 5a discontinuous mosaic


// 1b 5C black background
// 1b 5d new background    place text colour before 5d to choose color
// 1b 5e  hold mosaic
// 1b 23 release mosaic

// 1b 5d draws a bar?
// 1b 60 draws characterset 

int ttxMosaicLUT(char symbol){
    switch(symbol){
        case 0x21: return U'\U0001fb00'; break;
        case 0x22: return U'\U0001fb01'; break;
        case 0x23: return U'\U0001fb02';break;
        case 0x24: return U'\U0001fb03';break;
        case 0x25: return U'\U0001fb04';break;
        case 0x26: return U'\U0001fb05';break;
        case 0x27: return U'\U0001fb06';break;
        case 0x28: return U'\U0001fb07';break;
        case 0x29: return U'\U0001fb08';break;
        case 0x2a: return U'\U0001fb09';break;
        case 0x2b: return U'\U0001fb0a';break;
        case 0x2c: return U'\U0001fb0b';break;
        case 0x2d: return U'\U0001fb0c';break;
        case 0x2e: return U'\U0001fb0d';break;
        case 0x2f: return U'\U0001fb0e';break;
        case 0x30: return U'\U0001fb0f';break;
        case 0x31: return U'\U0001fb10';break;
        case 0x32: return U'\U0001fb11';break;
        case 0x33: return U'\U0001fb12';break;
        case 0x34: return U'\U0001fb13';break;
        case 0x35: return U'\u258c';break;
        case 0x36: return U'\U0001fb14';break;
        case 0x37: return U'\U0001fb15';break;
        case 0x38: return U'\U0001fb16';break;
        case 0x39: return U'\U0001fb17';break;
        case 0x3a: return U'\U0001fb18';break;
        case 0x3b: return U'\U0001fb19';break;
        case 0x3c: return U'\U0001fb1a';break;
        case 0x3d: return U'\U0001fb1b';break;
        case 0x3e: return U'\U0001fb1c';break;
        case 0x3f: return U'\U0001fb1d';break;
        
        case 0x60: return U'\U0001fb1e';break;
        case 0x61: return U'\U0001fb1f';break;
        case 0x62: return U'\U0001fb20';break;
        case 0x63: return U'\U0001fb21';break;
        case 0x64: return U'\U0001fb22';break;
        case 0x65: return U'\U0001fb23';break;
        case 0x66: return U'\U0001fb24';break;
        case 0x67: return U'\U0001fb25';break;
        case 0x68: return U'\U0001fb26';break;
        case 0x69: return U'\U0001fb27';break;
        case 0x6a: return U'\u2590';break;
        case 0x6b: return U'\U0001fb28';break;
        case 0x6c: return U'\U0001fb29';break;
        case 0x6d: return U'\U0001fb2a';break;
        case 0x6e: return U'\U0001fb2b';break;
        case 0x6f: return U'\U0001fb2c';break;
        case 0x70: return U'\U0001fb2d';break;
        case 0x71: return U'\U0001fb2e';break;
        case 0x72: return U'\U0001fb2f';break;
        case 0x73: return U'\U0001fb30';break;
        case 0x74: return U'\U0001fb31';break;
        case 0x75: return U'\U0001fb32';break;
        case 0x76: return U'\U0001fb33';break;
        case 0x77: return U'\U0001fb34';break;
        case 0x78: return U'\U0001fb35';break;
        case 0x79: return U'\U0001fb36';break;
        case 0x7a: return U'\U0001fb37';break;
        case 0x7b: return U'\U0001fb38';break;
        case 0x7c: return U'\U0001fb39';break;
        case 0x7d: return U'\U0001fb3a';break;
        case 0x7e: return U'\U0001fb3b';break;
        case 0x7f: return U'\u2588';break;

        default: return symbol;break;
    }
    return 0;
}

std::vector<std::string> file_getAll(std::string_view path){
    std::vector<std::string> contents;
    std::ifstream file(path.data(),std::ifstream::in|std::ifstream::binary);
    char temp[1000];
    if(file.good()){
        while(!file.eof()){
            file.getline(temp,1000);
            contents.push_back(temp);
        }
        file.close();
    }
    return contents;
}

std::string_view getControlByte(char ctrlChar){
    switch(ctrlChar){
        case 'n': return "\n";
        case 'r': return "\r";
        case 'e': return "\e";
        case '\\': return "\\";
    }
    return "";
}

int writeScreen(std::string file){
    bool writing = true;
    std::string filePath(std::format("{0:}pages/{1:}",path_exe,file));
    if(std::ifstream(filePath.data()).good()){
        std::string type = file.substr(file.size()-3);
        std::cout << std::format("{0:} Requested file {1:}. Type = {2:}\n",term_info,filePath,type);
        int bytesSent=0;
        bool graphicsMode=false;

        std::vector<std::string> page = file_getAll(filePath);

        //std::cout << std::format("{0:} First line is {1:}.\n",term_info,page[2].data());

        for(int i=0; i<page.size();i++){
            for(int j=0; j<page[i].length();j++){
                if(type=="vid"||type=="txt"){ //viditel formatted pages
                    switch(page[i][j]){
                        case '\\': j++;
                                if(page[i][j]=='#'){
                                    writing= !writing;
                                } else if(page[i][j]!='x'){
                                    if(writing){
                                        s.transmit(getControlByte(page[i][j]));
                                        if(page[i][j]!='r')printf("<\\%c>",page[i][j]);
                                        bytesSent++;
                                    }
                                } else {
                                    std::string compose(1,page[i][++j]);
                                    compose += page[i][++j];
                                    uint8_t byte = strtoul(compose.c_str(),nullptr,16);
                                    if(writing){
                                        s.transmit(std::string(1,byte));
                                        printf("<\\x%s>",compose.c_str());
                                        bytesSent++;
                                    }
                                }
                                break;
    
                        default: if(writing){
                                s.transmit(std::string(1,page[i][j]));
                                printf("%c",page[i][j]);
                                bytesSent++;
                        }
                        break;
                    }
                } else if(type=="tex" || type =="tx2"){ //direct 0x00-0x7F teletext (with \n newline support)    experimental TX2 (raw 20-9F) support?
                    if(type=="tx2") page[i][j]=page[i][j]&0b01111111;
                    switch(page[i][j]){
                        case '\\': j++;
                                if(page[i][j]=='n'){
                                    s.transmit("\n\r");printf("\n\r");
                                } else if(page[i][j]=='c'){
                                    s.transmit("\x0c");
                                } else if(page[i][j]=='D'){
                                    s.transmit("\x1b\x4d");printf(" ");bytesSent++;
                                } else if(page[i][j]=='d'){
                                    s.transmit("\x1b\x4c");printf(" ");bytesSent++;
                                } else {
                                    s.transmit("\\"+std::string(1,page[i][j]));
                                    printf("\\n\n\r");
                                } 
                        break;
                    /*
                      
                        case 0x01: s.transmit("\x1b\x41");printf("\e[1;31m ");bytesSent++;graphicsMode=false; break;
                        case 0x02: s.transmit("\x1b\x42");printf("\e[1;32m ");bytesSent++;graphicsMode=false; break;
                        case 0x03: s.transmit("\x1b\x43");printf("\e[1;33m ");bytesSent++;graphicsMode=false; break;
                        case 0x04: s.transmit("\x1b\x44");printf("\e[1;34m ");bytesSent++;graphicsMode=false; break;
                        case 0x05: s.transmit("\x1b\x45");printf("\e[1;35m ");bytesSent++;graphicsMode=false; break;
                        case 0x06: s.transmit("\x1b\x46");printf("\e[1;36m ");bytesSent++;graphicsMode=false; break;
                        case 0x07: s.transmit("\x1b\x47");printf("\e[1;37m ");bytesSent++;graphicsMode=false; break;
                        case 0x08: s.transmit("\x1b\x48");printf(" ");bytesSent++; break; //flashing
                        case 0x09: s.transmit("\x1b\x49");printf(" ");bytesSent++; break; //flashing off

                        case 0x0c: s.transmit("\x1b\x4c");printf(" ");bytesSent++; break; 
                        case 0x0d: s.transmit("\x1b\x4d");printf(" ");bytesSent++; break; 

                        case 0x11: s.transmit("\x1b\x51");printf("\e[1;31m ");bytesSent++;graphicsMode=true; break;
                        case 0x12: s.transmit("\x1b\x52");printf("\e[1;32m ");bytesSent++;graphicsMode=true; break;
                        case 0x13: s.transmit("\x1b\x53");printf("\e[1;33m ");bytesSent++;graphicsMode=true; break;
                        case 0x14: s.transmit("\x1b\x54");printf("\e[1;34m ");bytesSent++;graphicsMode=true; break;
                        case 0x15: s.transmit("\x1b\x55");printf("\e[1;35m ");bytesSent++;graphicsMode=true; break;
                        case 0x16: s.transmit("\x1b\x56");printf("\e[1;36m ");bytesSent++;graphicsMode=true; break;
                        case 0x17: s.transmit("\x1b\x57");printf("\e[1;37m ");bytesSent++;graphicsMode=true; break;
                        
                        case 0x19: s.transmit("\x1b\x59");printf(" ");bytesSent++; break; //contiguous mosaic 
                        case 0x1a: s.transmit("\x1b\x5a");printf(" ");bytesSent++; break; //discontiguous mosaic
                        case 0x1c: s.transmit(" \x1b\x5c");printf(" ");bytesSent++; break; //black bg
                        case 0x1d: s.transmit("\x1b\x5d");printf(" ");bytesSent++; break; // new background

                        
                        
                        
                    */

                        //condense this to if char < 0x20, transmit char+40?
    
                        default: 

                                if(page[i][j]>0x00&&page[i][j]<0x20 && (page[i][j]!=0x0a)){
                                    s.transmit("\x1b"); s.transmit(std::string(1,page[i][j]+0x40));

                                    if(page[i][j]>0x00&&page[i][j]<0x08){
                                        graphicsMode=false;
                                        std::cout << std::format("[1;3{0:}m",page[i][j]);
                                    }
                                    if(page[i][j]>0x10&&page[i][i]<0x18){
                                        graphicsMode=true;
                                        std::cout << std::format("\e[1;3{0:}m",page[i][j]-0x10);
                                    }
                                    printf(" ");
                                } else {
                                    s.transmit(std::string(1,page[i][j]));
                                }

                                
                                if(!graphicsMode){
                                    printf("%c",page[i][j]);
                                } else {
                                    printf("%lc",ttxMosaicLUT(page[i][j]));
                                    
                                }
                                bytesSent++;
                                if(bytesSent%40==0){
                                    printf("\n\r\e[0m");
                                    graphicsMode=false;

                                }
                        break;
                    }
                }
            }
        }
        std::cout << std::format("\n\n{0:} Printed page {1:}, {2:} bytes sent.\n",term_ok,file,bytesSent);
        return 0;
    } else {
        std::cout << std::format("\n\n{0:} File not found: {1:}.\n",term_warn,filePath);
        return 1;
    }
    return 0;
}

void getCursor(int* x, int* y){
    printf("\e[6n");
    fflush(stdin);
    scanf("\e[%d;%dR",x,y);
}

void service_setCursor(int x, int y){
    if(x>23)x=23;
    if(y>39)y=39;
    s.transmit("\x1e");
    for(int i=0; i<x; i++){
        s.transmit("\n");
    }
    for(int i=0; i<y; i++){
        s.transmit("\x09");
    }
}

std::string service_getPage(){
    std::string output;
    char val;
    do{
        val=s.getKey()[0];
        output.append(std::string(1,val));
        s.transmit(std::string(1,val));
        printf("%02x",val);
    }while(val!='_');
    printf("\n");
    return output;
}


int service_main(){
    char val;
    std::string_view currentPage="";
    std::string_view currentDomain="gateway";
    std::string_view currentSubDomain="";
    bool hubredirect=true;
    bool shouldDraw=true;
    do{
        if(currentDomain=="gateway"){
            if(shouldDraw==true){
                writeScreen("system/index.txt");
            }
            shouldDraw=true;
            std::string request = service_getPage();
            printf("requested at gateway \"%s\"",request.data());
            if(request=="*000_"){
                currentDomain="creatureTel";
                currentPage="000";
                hubredirect=true;
            } else if(request=="*100_"){
                currentDomain="timeMachine";
            } else if(request=="*500_"){
                currentDomain="exhibition";
            } else {
                s.transmit("\e\x1e\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a Page not implemented!");
                shouldDraw=false;
            }



        } else if(currentDomain=="creatureTel"){
            if(hubredirect){
                writeScreen(std::format("creatureTel/{0:}.txt", currentPage));
                hubredirect=false;
            }
            std::string request = service_getPage();
            printf("Request=%s",request.data());
            std::string rqclean = request.substr(1,request.size()-2).data();
            std::string rqpath = std::format("creatureTel/{0:}.txt", rqclean).data();
            std::string rqpath2 = std::format("creatureTel/{0:}.tx2", rqclean).data();
            printf("\nrequested at CreatureTel \"%s\"",rqpath.data());
            

            std::string fullPath(std::format("{0:}pages/{1:}",path_exe,rqpath));
            std::string fullPath2(std::format("{0:}pages/{1:}",path_exe,rqpath2));
            if(std::ifstream(fullPath).good()){
                currentPage=rqclean.data();
                writeScreen(std::format("creatureTel/{0:}.txt", currentPage));
            } else if(std::ifstream(fullPath2).good()){
                currentPage=rqclean.data();
                writeScreen(std::format("creatureTel/{0:}.tx2", currentPage));
            } else if(request=="*_"||request=="_"){
                if(currentPage=="000"){
                    s.transmit("\x1e\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\e\x42\e\x5d\e\x47 Return to gateway? [y,n] ");
                    if(s.getKey()[0]=='y'){
                        currentDomain="gateway";
                        hubredirect=false;
                    }
                }
            } else {
                s.transmit(std::format("\x1e\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a\x0a Page not found: {0:}.",request.data()));
                std::cout << std::format("\n\n{0:} File not found: {1:}.\n",term_warn,request.data());

            }


        } else if(currentDomain=="timeMachine"){

        } else if(currentDomain=="theGallery"){

        } else if(currentDomain=="guestBook"){

        } else if(currentDomain=="teleGids"){

        } else if(currentDomain=="exhibition"){

            
            int pagenr=0;
            std::vector<uint8_t> received_data;
            std::string finalPath;
            bool foundFile =false;
            std::string rqpath;
            std::string fullPath;
            while(true){
                std::string request = std::format("{0:0>3}",pagenr);
                printf("Request=%s",request.data());
                foundFile=false;
                if(foundFile==false){
                    rqpath = std::format("exhibition/{0:}.txt", request).data();
                    fullPath = std::format("{0:}pages/{1:}",path_exe,rqpath);
                    if(std::ifstream(fullPath).good()){
                        finalPath = rqpath;
                        foundFile = true;
                    } 
                }
                if(foundFile==false){
                    rqpath = std::format("exhibition/{0:}.vid", request).data();
                    fullPath = std::format("{0:}pages/{1:}",path_exe,rqpath);
                    if(std::ifstream(fullPath).good()){
                        finalPath = rqpath;
                        foundFile = true;
                    } 
                }
                if(foundFile==false){
                    rqpath = std::format("exhibition/{0:}.tex", request).data();
                    fullPath = std::format("{0:}pages/{1:}",path_exe,rqpath);
                    if(std::ifstream(fullPath).good()){
                        finalPath = rqpath;
                        foundFile = true;
                    }
                }
                if(foundFile==false){
                    rqpath = std::format("exhibition/{0:}.tx2", request).data();
                    fullPath = std::format("{0:}pages/{1:}",path_exe,rqpath);
                    if(std::ifstream(fullPath).good()){
                        finalPath = rqpath;
                        foundFile = true;
                    }
                }
                if(foundFile==true){
                    printf("\nrequested at Exhibition mode \"%s\"",rqpath.data());
                    writeScreen(finalPath);
                    pagenr++;
                } else {
                    pagenr=0;
                    continue;
                }


                auto future = s.receiveAsync(1, 20000, "q");
                received_data = future.get();
        
        
                //if(verbose)while(future.wait_for(std::chrono::milliseconds(500)) != std::future_status::ready)
                //std::cout << '.' <<std::flush;

                if(received_data.empty()!= true){
                    if(received_data[0] == 'q'){
                        currentDomain="gateway";
                        break;
                    }else if(received_data[0]==' '){
                        //pagenr++;
                    }
                }
               
            }
        

        } else if(currentDomain=="presentation"){ //remove this later!
            int pagenr=0;
            while(true){
                std::string request = std::format("{0:0>3}",pagenr);
                printf("Request=%s",request.data());
                std::string rqpath = std::format("presentation/{0:}.tex", request).data();
                printf("\nrequested at Presentation \"%s\"",rqpath.data());
                
                std::string fullPath(std::format("{0:}pages/{1:}",path_exe,rqpath));
                if(std::ifstream(fullPath).good()){
                    writeScreen(rqpath);
                    pagenr++;
                } else { 
                    rqpath = std::format("presentation/{0:}.tx2", request).data();
                    fullPath = std::format("{0:}pages/{1:}",path_exe,rqpath);
                    if(std::ifstream(fullPath).good()){
                        writeScreen(rqpath);
                        pagenr++;
                    } else {
                        currentDomain="gateway";
                        break;
                    }
                }
                s.getKey();
            }
        } else {
            currentDomain="gateway";

        }

        //writeScreen("system/test.txt");
        //val=s.getKey()[0];
    } while(val!='q');
    /*
    s.transmit("\x0c");
    s.transmit("Welcome to Viditel!");
    
    s.transmit("\n\r1234567890123456789012345678901234567890");
    s.transmit("1234567890123456789012345678901234567890");
    s.transmit("1234567890123456789012345678901234567890");
    s.transmit("1234567890123456789012345678901234567890");
    s.transmit("\x1f\x49\x42HALLOOOOO");
    s.transmit("\x1b\x12\x50G");
    s.transmit("\n\r\n\r");
    s.transmit("\x1b\x1a\x20\x03""A halloOOoodiooo \x1b\x49 doeiii\n\r");
    s.transmit("\x1b\x1a\x12""A halloOOoodiooo \x1b\x49 doeiii\n\r");
    s.transmit("\x1b\x1a\x21""A halloOOoodiooo \x1b\x49 doeiii\n\r");
    s.transmit("\x1b\x1a\x32""A halloOOoodiooo \x1b\x49 doeiii\n\r");
    s.transmit("\x1b\x44\x1b\x5d\x1b\x47""A halloOOoodiooo \x1b\x5C doeiii\n\r");
    s.transmit("\x1b\x5e\x1b\x55""sjeorigjseorigjseorisjoeirjg \x1b\x23 doeiii\n\r");
    s.transmit("\x1b\x5e\x1b\x55\x1b\x59""sioerghseiroghsoerighseor \x1b\x23 doeiii\n\r");
    s.transmit("\x1b\x1a\x61""A halloOOoodiooo \x1b\x49 doeiii\n\r");
while(true){
            auto future = s.receiveAsync(1);
            std::vector<uint8_t> hauh = future.get();
            if(!hauh.empty())break;
            printf("doeoidoeiiiiii");
        }
    int joepie = 27;
    int soepie = 0;

    while(true){
        s.transmit(std::format("\x0cNow with 0x{:x} 0x{:x}\n\r\n\r",joepie, soepie));


        char soup[2] = {0,0};
        soup[0]=joepie&255;
        s.transmit(std::string_view(soup));
        soup[0]=soepie&255;
        s.transmit(std::string_view(soup));

        for(int i=31; i<128; i++){
            soup[0]=i&255;
            s.transmit(std::string_view(soup));
        }

        while(true){
            auto future = s.receiveAsync(1);
            std::vector<uint8_t> hauh = future.get();
            if(!hauh.empty())break;
            printf("doeoidoeiiiiii");
        }
        if(++soepie>127){
            soepie=0;
            joepie++;
        }
        if(joepie>32) break;
    }
    */
    return 0;
}


char termGetch(){
    
    if(std::cin.tellg()>0){
        return std::cin.get();
    }
    printf("nope");
    return 0;
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





int main(int argc, char** argv){
    setlocale(LC_ALL,"");
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    std::string tempPath(result, (count>0)?count:0);
    tempPath.erase(tempPath.begin()+tempPath.find_last_of('/')+1,tempPath.end());
    path_exe = tempPath;

    fcntl(0,F_SETFL,fcntl(0,F_GETFL) | O_NONBLOCK);

    printf("\e[2J\e[1;1H\n *** Viditel Playground ***\n\n");

    int x,y;
    //getCursor(&x,&y);

    std::cout << std::format("{0:} location is {1:},{2:}\n",term_def,x,y);

    std::cout << std::format("{0:} Opening serial port\n",term_def);

    if(s.isOpen()){
        termFillPrevDef(term_fail);
        std::cout << std::format("{0:} Serial port already open",term_empty);
        return 100;
    }
    try{
	    s.open("/dev/ttyS1", 1200U, serial::Serial::flowControlType::none, 7U, serial::Serial::parityType::even);
    } catch(boost::system::system_error &e){
        termFillPrevDef(term_fail);
        std::cout << std::format("{0:} Could not open serial port: {1:}\n\n",term_empty, e.code().message());
        return 101;
    }

    if(!s.isOpen()){
        termFillPrevDef(term_fail);
        std::cout << std::format("{0:} Serial port wasn't opened for unknown reason\n\n",term_empty);
        
    } else {
        termFillPrevDef(term_ok);
    }
    

    modemReset();
    


    //modemCommand("ATDT56",true);
    std::vector<uint8_t> received_data;
    std::string_view cleanData;
    bool running = true;
    short waitingSymbolIndex = 0;
    short waitCount;
    while(running){
        received_data.clear();
        cleanData="";
        waitCount = 0;
        std::cout << std::format("\n{0:} Waiting for connection...  ",term_empty);
        fflush(stdin);
        do{
            if(termAvailable()){
                std::cout <<"WAUW";
                char c = std::cin.get();
                if(c=='q'){
                    return 0;
                } else {
                    std::cout << std::format("\n{0:} --- INPUT \'{1:}\' pressed  ",term_info,c);
                }
            }

            auto future = s.receiveAsync(40, 500, "RING\r\n");
            while(future.wait_for(std::chrono::milliseconds(99)) != std::future_status::ready)
                std::cout << std::format("\e[1D{0:}",waitingSymbol[waitingSymbolIndex=(waitingSymbolIndex+1)%4]) <<std::flush;
	        received_data = future.get();
            cleanData = {reinterpret_cast<const char * const>(received_data.data()), received_data.size()};

        } while (cleanData.find("RING") == std::string::npos);

        std::cout << std::format("\n{0:} Incoming call, answering...  ",term_empty);
        //s.transmit("ATA");

        do{
            auto future = s.receiveAsync(40, 1000, "CONNECT 1200\r\n");
            while(future.wait_for(std::chrono::milliseconds(99)) != std::future_status::ready){
                std::cout << std::format("\e[1D{0:}",waitingSymbol[waitingSymbolIndex=(waitingSymbolIndex+1)%4]) <<std::flush;
                waitCount++;
            }
	        received_data = future.get();
            cleanData = {reinterpret_cast<const char * const>(received_data.data()), received_data.size()};

        } while (cleanData.find("CONNECT") == std::string::npos && waitCount <2000);
        
        if(cleanData.size() <1){
            std::cout << std::format("{0:} Handshake did not succeed\n",term_fail);
        }
        if(cleanData.find("CONNECT") == std::string::npos){
            std::cout << std::format("{0:} Handshake did not succeed: {1:}\n",term_fail,cleanData);
        }
        std::cout << std::format("\n{0:} {1:}\n",term_modemComm,cleanData);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::cout << std::format("{0:} Connection established successfully, handing off to service function...\n",term_ok);

        service_main();

        std::cout << std::format("{0:} Service function exited. Hanging up and preparing for new connection...\n",term_info );
        modemHangup();

        modemReset();

    }//std::cin.rdbuf()->in_avail();


	s.close();

    printf("\n\n \e[1;31m*****\e[0m Server stopped, goodbye!\n");

    return 0;
}