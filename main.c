#include <stdio.h>
#include "pico/stdlib.h"

#include <pico/float.h>
#include <inttypes.h>
#include <math.h>
//#include"pico/bootrom.h"

#define BUFLEN      128
#define TMPSLEN     12

#define LEDPIN 25


struct msgInfo{
    uint32_t status;        // 0xRR RR CC FF, RR == for future use, CC == connected sats, FF == fix level
    uint32_t time;          // 0xZZ HH MM SS, Signed Zone offset byte (0x00 == UTC), HH == 0-24h, MM == 0-60m, SS = 0-60s
    int32_t lat;
    int32_t lon;
    float lat_f;
    float lon_f;
};

typedef struct msgInfo msgInfo;

struct processReturn{
    uint32_t fixedVal;
    float floatVal;
};

typedef struct processReturn processReturn;


void clearBuffer(char *buff2clr){
    for(int n = 0; n < BUFLEN; n++){
        buff2clr[n] = 0x00;
    }
}

void parseMsg(char *inputStr, msgInfo *message, msgInfo *lastParsed);
uint32_t str_to_uint32(char *input);
void printStruc(msgInfo *message);
void ddmmmm_2_return(char *arr, int *startPt, processReturn *returnArr);
int32_t fl_2_fi(float *input);




int main(){
    stdio_init_all();

// Pog
    uart_init(uart1, 9600);
    gpio_set_function(8, GPIO_FUNC_UART);       // TX1
    gpio_set_function(9, GPIO_FUNC_UART);       // RX1
// 

    gpio_init(LEDPIN);
    gpio_set_dir(LEDPIN, GPIO_OUT);


    char tmp[BUFLEN] = {0x00};                  // Incoming message buffer

    msgInfo currMsg;                     // Data broken out from string
    msgInfo lastMsg;                     // Last message

    currMsg.status = 0;
    currMsg.time = 0;
    currMsg.lat = 0;
    currMsg.lon = 0;
    currMsg.lat_f = 0;
    currMsg.lon_f = 0;

    uint32_t UPTIME = 0;

    sleep_ms(1000);
    printf("Startup Complete!\n");

    for(int n = 0; n < 4; n++){
        printf("Hi! %i\n", n + 1);

        gpio_put(LEDPIN, 1);
        sleep_ms(750);
        gpio_put(LEDPIN, 0);
        sleep_ms(750);
    }

    int ctr = 0;
    
    while(1){
        
        if(uart_is_readable(uart1)){                                        // If message is present in buffer
            /*()
            uart_read_blocking(uart1, tmp, BUFLEN);
            //printf("%128s\n", tmp);
            printf("%s\n", tmp);
            */

            int inner_ct = 0;                                               // Index for char input array
            char last_read = uart_getc(uart1);                              // Read in initial character
            while(last_read && last_read != '\n' && last_read != 0x04){     // Validate character is NOT 'end of transmission' (0x04) or '\n'
                // Read in until newline or EOT (end of transmission)
                tmp[inner_ct] = last_read;                                  //
                last_read = uart_getc(uart1);                               // Blocking get char from UART stream
                inner_ct += 1;                                              // Increment read array index
            }
            
            if(tmp[0] == '$' && tmp[4] == 'G' && tmp[5] == 'A'){            // Parse GNGGA messages
                printf("%s\n", tmp);                    // Print read GNGGA string

                parseMsg(tmp, &currMsg, &lastMsg);      // Parse message and save data to currMsg and update lastMsg structures

                printStruc(&currMsg);                   // Print raw data contents of last parsed message, then print readable formatted data 


                if(tmp[7] != ','){          // If we at least have time data
                    UPTIME += 1;            // Increment uptime counter ( 1 pulse per second )
                } else {
                    UPTIME = 0;             // Reset time if we lose at least time fix
                }

                printf("Uptime:\t%us\n\n", UPTIME);
            }
          
            clearBuffer(tmp);               // Clear input buffer, un-needed once long term fix has been achieved and str length is constant/solely-increasing
        }
        
    }

}


void parseMsg(char *inputStr, msgInfo *message, msgInfo *lastParsed){
    char tmps[TMPSLEN] = {0x00};                                        // Temp array to read individual groups from GNGGA string
    uint32_t summr[3] = {0};                                            // Possibly un needed

    processReturn returningArray;                                       // Initialize return type array such that fixed and float types can be returned from a function

    lastParsed->time = message->time;                                   // Update last message struct, zero out current structure
    message->time = 0;

    lastParsed->status = message->status;
    message->status = 0;

    lastParsed->lat = message->lat;
    message->lat = 0;

    lastParsed->lon = message->lon;
    message->lon = 0;

    lastParsed->lat_f = message->lat_f;
    message->lat_f = 0;

    lastParsed->lon_f = message->lon_f;
    message->lon_f = 0;

    if(inputStr[1] == 'G' && inputStr[7] != ','){           // If string is valid ($GPGGA expected)
    /*
        for(int n = 0; n < 3; n++){                     // Find valid time
            // Base address for time => arr[7]
            tmps[0] = inputStr[7 + (2 * n)];
            tmps[1] = inputStr[8 + (2 * n)];
            summr[n] = str_to_uint32(tmps);
            
            printf("%s ", tmps);

            message->time |= str_to_uint32(tmps) << (8 * (2 - n));
        }

        printf("\n");

        printf("Time: %i\n", message->time);
        printf("%ih %im %is UTC\n", (message->time >> 16) & 0xFF, (message->time >> 8) & 0xFF, (message->time) & 0xFF); 
*/
        int ct = 0;
        int term = 0;
        int inner_ctr = 0;
        int in_ov_ctr = 0;
        while(inputStr[ct]){
            // Expected Valid $GPGGA string
            //  ID      Time        Lat         Lon     Fix Sat Count
            // $GPGGA,hhmmss.ss,ddmm.mmmm,n,dddmm.mmmm,e,q,ss,y.y,a.a,z,g.g,z,t.t,iiii*CC
            //  0       1       2         3     4      5 6  7  8   9  10 11 12  13  14  
            // Terms are delimeted by commas. ex. $GPGGA == term 0, hhmmss.ss == term 1 .....
            if(inputStr[ct] == ','){            // Identify which term we are in.
                term += 1;
                inner_ctr = 0;
                in_ov_ctr = 0;
                ct += 1;
            }

            switch(term){
                case 0:
                    // ID
                break;
                
                case 1:
                    // Time
                    if(inputStr[ct] == '.'){
                        break;
                    }

                    // Think of time as 4 groups of 2 chars. HH MM SS.SS
                    tmps[inner_ctr] = inputStr[ct];         // Load first char from group
                    inner_ctr += 1;                         // Index to 2nd char in temp array
                    ct += 1;                                // Index to 2nd char in string
                    tmps[inner_ctr] = inputStr[ct];         // Load 2nd char from group
                    inner_ctr = 0;                          // Reset temp array index for next group
                    
                    //printf("Time %s\n", tmps);            // Debug

                    message->time |= str_to_uint32(tmps) << (8 * (3 - in_ov_ctr));
                    in_ov_ctr += 1;                         // Index which group we're on
                break;

                case 2:
                    // Lat
                    /*while(inputStr[ct] != ','){
                        tmps[inner_ctr] = inputStr[ct];
                        inner_ctr += 1;
                        ct += 1;
                    }
                    printf("Lat: %s strLen: %i\n", tmps, inner_ctr);
                    inner_ctr = 0;
                    ct -= 1;
                    */
                    ddmmmm_2_return(inputStr, &ct, &returningArray);
                    message->lat_f = returningArray.floatVal;
                    message->lat = returningArray.fixedVal;
                break;
                
                case 3:
                    // direction ( N, S ), N = +, S = -
                    if(inputStr[ct] == 'S'){
                        message->lat *= -1;
                        message->lat_f *= -1.0f;
                    } else {
                        // Leave 31th bit 0
                    }
                break;
                
                case 4:
                    // Lon
                    ddmmmm_2_return(inputStr, &ct, &returningArray);
                    message->lon_f = returningArray.floatVal;
                    message->lon = returningArray.fixedVal;
                break;
                
                case 5:
                    // direction ( E, W ), E = +, W = -
                    if(inputStr[ct] == 'W'){
                        message->lon *= -1;
                        message->lon_f *= -1.0f;
                    } else {
                        // Leave 31th bit 0
                    }
                break;
                
                case 6:
                    // GPS Fix Indicator. 0 = no fix, 1 = Fix, 2 = Differential Fix
                    tmps[inner_ctr] = inputStr[ct];
                    message->status |= str_to_uint32(tmps);
                break;
                
                case 7:
                    // Number of sats being used (0 - 12)
                    tmps[inner_ctr] = inputStr[ct];
                    inner_ctr += 1;
                    ct += 1;
                    tmps[inner_ctr] = inputStr[ct];

                    message->status |= str_to_uint32(tmps) << 8;
                break;
                
                case 8:
                    // Horizontal Dilution of Precision (HDOP) dependent on sat positioning relative to eachother
                    // <1 == Best, 1-2 == Great, 2-5 == ok, past this I'm unsure if data should be kept
                break;
                
                case 9:
                    // Antenna Height
                break;
                
                case 10:
                    // Antenna Height Units (M = meters)
                break;

                default:
                    return;                         // Parse terminates here.
                break;
            }

            for(int t = 0; t < TMPSLEN; t++){
                tmps[t] = 0x00;
            }

            ct += 1;
        }
    }
}

uint32_t str_to_uint32(char *input){
    int t_ct = 0;
    int scalar = 1;
    uint32_t sum = 0;

    while(input[t_ct]){
        t_ct += 1;
    }

    // char '0' == 0x30, char '9' == 0x39
    for(int n = 0; n < t_ct; n++){
        sum += (input[(t_ct - 1) - n] - 0x30) * scalar;
        scalar *= 10;
    }

    return sum;
}

void printStruc(msgInfo *message){
    printf("Time: %i\t", message->time);
    printf("%ih %im %i.%is UTC\n", (message->time >> 24) & 0xFF, (message->time >> 16) & 0xFF, (message->time >> 8) & 0xFF, (message->time) & 0xFF);

    printf("Status: %i\t", message->status);
    printf("Fix: %i, Sats: %i\n", (message->status) & 0xFF, (message->status >> 8) & 0xFF);

    printf("Lat Fixed: %u\tLat Float: %.6f\n", message->lat, message->lat_f);

    printf("Lon Fixed: %u\tLon Float: %.6f\n", message->lon, message->lon_f);

    printf("\n");
}

void ddmmmm_2_return(char *arr, int *startPt, processReturn *returnArr){
    int eos = 0;                                                            // Identify string length (end of string)
    int dec_pt = 0;                                                         // Identify decimal point position within string

    char temp_dec[8] = {0x00};                                              // Temporary array for splitting sections from string DD MM .MMMM

    while(arr[eos + *startPt] != ','){                                      // eos == incrementing index, startPt == base address of string to read
        if(arr[eos + *startPt] == '.'){                                     // Identify position of decimal point
            dec_pt = eos + *startPt;
        }
        eos += 1;                                                           // Increment string length counter (eol)
    }

    // Isolate Integer Degrees
    int inner_ct = 0;                                                       // Index for temp array
    for(int n = *startPt; n < (dec_pt - 2); n++){                           // cut out elements 0 - (dec_pt - 2) as format is either DDDMM.MMMM or DDMM.MMMM 
        temp_dec[inner_ct] = arr[n];                                        //      this allows 2 and 3 digit Degree indicators to be read properly
        inner_ct += 1;
    }

    uint32_t degrees = str_to_uint32(temp_dec);                             // Convert temporary string containing DD or DDD into an unsigned integer
    for(int n = 0; n < 4; n++){                                             // Zero out front part of temp array as there is potential for MM to be shorter than DD or DDD
        temp_dec[n] = 0x00;
    }

    // Isolate Integer Minutes
    temp_dec[0] = arr[dec_pt - 2];                                          // Read in MM from DD[MM].MMMM
    temp_dec[1] = arr[dec_pt - 1];

    uint32_t minutes_dec = str_to_uint32(temp_dec);                         // Convert MM. to unsigned integer

    // Isolate Fractional Minutes
    inner_ct = 0;                                                           // Reset array index
    for(int n = (dec_pt + 1); n < (*startPt + eos); n++){                   // Read in characters from beyond the decimal point (fractional minutes component)
        temp_dec[inner_ct] = arr[n];
        inner_ct += 1;
    }

    uint32_t minutes_f = str_to_uint32(temp_dec);                           // Convert fractional minutes to int assuming decimal point doesn't exist ( ie 0.12345 -> 12345. )

    float scalar = 1.0f;
    for(int n = 0; n < ((*startPt + eos - 1) - dec_pt); n++){               // Count number of values beyond decimal point
        scalar *= 10.0f;                                                    // Create scalar to appropriately shift fractional component
    }

    float sum_ = (float)degrees;                                                // Create float version of decimal degrees (uint32 -> float)
    sum_ += (((float)minutes_dec) + (((float)minutes_f) / scalar)) / 60.0f;     // ( minutes + (frac_minutes / 100....) ) / 60 == decimal degrees. DMS -> Deg

    // Debug
    //printf("Deg: %i  Min: %i Min_f: %i Min_F: %.6f\n", degrees, minutes_dec, minutes_f, (float)minutes_f / (scalar));
    //printf("Val: %.6f\n", sum_);
    //printf("Start = %i DecP = %i End = %i\n", *startPt, dec_pt, *startPt + eos);


    // Termination.
    *startPt += eos - 1;                    // Resets global loop counter to the end of the string ( -1 as the loop will increment this value )
    returnArr->floatVal = sum_;             // Passes values back through shared return struct
    returnArr->fixedVal = fl_2_fi(&sum_);   // ^^
    //returnArr->fixedVal = 0x00;             // Pass zero until fixed point is figured out more...
}


// The implementation of this is subject to change
int32_t fl_2_fi(float *input){
    int32_t temp_sum;

    // SDDD DDDD DFFF FFFF FFFF FFFF FFFF FFFF
    // Resolution of 1.192e-7
    // Integer Range of +/- 255

    temp_sum = (int32_t)*input;

    //printf("Integer Part:\t%i\n", temp_sum);

    *input -= (float)temp_sum;

    //printf("Frac Part:\t%.6f\n", *input);

    *input *= 10000000.0f;

    temp_sum = temp_sum << 23;

    temp_sum += (int32_t)*input;

    return temp_sum;
}
