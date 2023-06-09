#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xtimer.h"
#include "mutex.h"
#include "time.h"

#include "behaviors.h"
#include "sample_generator.h"
#include "drivers_sx127x.h"
#include "payload_formatter.h"
#include "semtech-loramac.h"

#include "app_debug.h"

/* Check payload_formatter for more details */
#define VALUE_MAXIMUM_LENGTH 14
#define LOGIC_TIME_MAXIMUM_LENGTH 5

/* Leakage */
#define LEAKAGE_CONDITION 0 /* L/min */

/* Setting TTN parameters */
#define DEV_EUI "70B3D57ED005D1D6"
#define APP_EUI "0000000000000011"
#define APP_KEY "5F129D225F930EB831FBE861B3B307D0"

uint32_t LEAKAGE_TEST_PERIOD = US_PER_SEC * 20;
uint32_t LATENCY_P2P = US_PER_SEC * 0;

int tx_complete_child;
char message[20];

int source_lora_ttn(node_t node) 
{
    puts("Beahvior: source_lora_ttn");

    /* json to publish on TTN */
    char json[128];

    /* Sampling time */
    int s_time = -1;
    /* Current date and time */
    //char datetime[20];
    //time_t current;

     /* Set TTN application parameters */
    char* deveui_list[4] = {"loramac", "set", "deveui", DEV_EUI};
    char** argv = (char**)&deveui_list;
    loramac_handler(4,argv);

    char* appeui_list[4] = {"loramac", "set", "appeui", APP_EUI};
    argv = (char**)&appeui_list;
    loramac_handler(4,argv);

    char* appkey_list[4] = {"loramac", "set", "appkey", APP_KEY};
    argv = (char**)&appkey_list;
    loramac_handler(4,argv);

    char* dr_list[4] = {"loramac", "set", "dr", "5"};
    argv = (char**)&dr_list;
    loramac_handler(4,argv);

    char* join_list[3] = {"loramac", "join", "otaa"};
    argv = (char**)&join_list;
    loramac_handler(3,argv);

    while(1) {
        /*time(&current);
        struct tm* t = localtime(&current);
        int c = strftime(datetime, sizeof(datetime), "%Y-%m-%d %T", t);
        if(c == 0) {
            printf("Error: invalid format.\n");
            return -1;
        }*/
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        //printf("now: %d/%02d/%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        sprintf(message, "%d/%02d/%02d %02d:%02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);         
        

        /* Set time for sampling: [0, inf) */
        s_time = (s_time + 1);

        /* Get water flow value */
        int water_flow = get_water_flow(node.node_type, 0, s_time);
        /* Fill json document */
        sprintf(json, "{\"node_id\": \"%s\", \"datetime\": \"%s\", \"water_flow\": \"%d\"}", node.node_self, message, water_flow);
        
        puts(json);

        /* Send a message 
        char* send_list[2] = {"send_cmd", json};
        argv = (char**)&send_list;
        send_cmd(2, argv);*/

        char* tx_list[3] = {"loramac", "tx", json};
        argv = (char**)&tx_list;
        loramac_handler(3, argv);

        /* Sleeping for five seconds */
        xtimer_sleep(5);

        /* AWS integration */
        /* TTNClient = mqtt.Client()
        AWSClient = AWSIoTMQTTClient("TTNbridge")

        // Connect the clients
        TTNClient.connect("eu.thethings.network", 1883, 60)
        AWSClient.connect()
        TTNClient.username_pw_set("<application-id>", "<access-key>")
        */
    }

    return 0;
}

static void _start_listening (void) 
{
    /* listen for lora messages */
    char* list[1] = {"listen_cmd"};
    char** argv = (char**)&list;
    listen_cmd(1, argv);
}

static void _sample (sample_t* sample, node_t node, int time) 
{
    /* Check water flow for each sensor and send a message to its children if any */
    sample->water_flow = (int*)malloc(sizeof(int)*node.children_count);
    sample->water_flow_sum = 0;
    for (int i = 0; i < node.children_count; i++) {
        /* Sample */
        sample->water_flow[i] = get_water_flow(node.node_type, i, time);
        if (APP_DEBUG) printf("Sensor %d, value: %d\n", i, sample->water_flow[i]);
        /* Sum */
        sample->water_flow_sum += sample->water_flow[i];
    }
    if (APP_DEBUG) printf("Sum: %d\n", sample->water_flow_sum);
}

static void _send_water_flow_to_children(node_t node, int time) 
{    
    /* Get the value of the water flow */
    sample_t sample;
    _sample(&sample, node, time);

    if (sample.water_flow_sum) {

        if(APP_DEBUG) printf("Water flow sum: %d\n\n", sample.water_flow_sum);

        /* Convert the time from int to string */
        char str_time[LOGIC_TIME_MAXIMUM_LENGTH];
        sprintf(str_time, "%d", time); 
        /* Convert the water flow from int to char* and split it between children */
        char** str_water_flow = (char**)malloc(sizeof(char*));
        for (int i = 0; i < node.children_count; i++) {
            str_water_flow[i] = (char*)malloc(sizeof(char)*VALUE_MAXIMUM_LENGTH);
            sprintf(str_water_flow[i], "%d", sample.water_flow[i]);
        }

        free(sample.water_flow);
        char* str_payload = NULL;

        /* Send water flow to children */
        for (int i = 0; i < node.children_count; i++) {
            tx_complete_child = 0;
            str_payload = format_payload(str_water_flow[i], node.node_self, node.node_children[i], "V", str_time);
            char* list[2] = {"send_cmd", str_payload};
            char** argv = (char**)&list;
            send_cmd(2, argv);

            /* Wait for transmission complete */
            while (!tx_complete_child) {
                /* The sendere thread has less priority, so we need to sleep a little bit */
                xtimer_msleep(100);
            }
            
            
        }

        /* Restart listening */
        _start_listening();

        free(str_payload);
        /* Free memory */
        for (int i = 0; i < node.children_count; i++) {
            free(str_water_flow[i]);
        }
        free(str_water_flow);
    }
}

void _check_leakage (node_t node, payload_t* payload) {
    /* Get the value of the water flow */
    sample_t sample;
    _sample(&sample, node, atoi(payload->logic_time));
    free(sample.water_flow);
    printf("Current water flow: %d. ", sample.water_flow_sum);

    /* Compute the difference */
    int difference = atoi(payload->value) - sample.water_flow_sum;

    if (difference > LEAKAGE_CONDITION) {
        /* Leakage detected */
        puts("leakage detected, sending a message to the source");

        /* Convert the differece from int to char* */
        char str_difference[VALUE_MAXIMUM_LENGTH];
        sprintf(str_difference, "%d", difference);

        /* Wait for source switch to listen mode */
        if (strcmp(node.node_father, node.node_source_p2p) == 0) {
            xtimer_msleep(100);
        }

        /* Send a message to the source */
        char* str_payload = format_payload(str_difference, node.node_self, node.node_source_p2p, "L", payload->logic_time);
        char* list[2] = {"send_cmd", str_payload};
        char** argv = (char**)&list;
        send_cmd(2, argv);

        /* Restart listening */
        _start_listening();

        /* Free memory */
        free(str_payload);

    } else {
        puts("No leakage detected\n");
    }
}

void transmission_complete_clb (void) {
    if (APP_DEBUG) puts("Callback on trasmission complete");
    tx_complete_child = 1;
}

void message_received_clb (node_t node, char message[32]) {
    if (APP_DEBUG) puts("Callback invoked, starting message parsing");

    if (strlen(message) > 31) {
        printf("Extraneous message received, message lenght: %d\n", strlen(message));
        return;
    }

    /* Message parsing */
    payload_t* payload = get_values(message);
    if (!payload) {
        /* Not a message from our application */
        if (APP_DEBUG) puts("Not a message from our application");

        free_payload(payload);
        return;
    }

    /* Check destination */
    if (strcmp(payload->to, node.node_self) != 0) {
        /* Message not sent to me */
        if (APP_DEBUG) puts("Message not sent to me");

        free_payload(payload);
        return;
    }
    
    /* Compute the sender of the message */
    if (strcmp(payload->from, node.node_father) == 0) {
        /* Message sent from the parent */
        printf("Message from the parent received: %s\n", message);

        /* Check leakage */
        _check_leakage(node, payload);

        free_payload(payload);
        return;
    }

    /* The CHIEF receive all the leakage messages */
    if (node.node_type == 1 && strcmp(payload->is_leak, "L") == 0) {
        printf("Message of leakage received: %s\n\n", message);

        /* UART send message to SOURCE TTN*/

        free_payload(payload);
        return;        
    }

}

int lora_p2p(node_t node) {
    puts("Behavior: lora_p2p");
    
    xtimer_ticks32_t last_wakeup;
    bool is_last_wakeup = false;
    int time = 5;

    _start_listening();

    while (1) {
        /* Set time for sampling: [0, inf) */
        time = (time + 1);

        /* BRANCH doesn't have children */
        if (node.node_type != 3) _send_water_flow_to_children(node, time);

        /* Duty cycle */
        if (!is_last_wakeup) {
            /* set last_wakeup only the first time */
            is_last_wakeup = true;
            last_wakeup = xtimer_now();
        }
        xtimer_periodic_wakeup(&last_wakeup, LEAKAGE_TEST_PERIOD);
    }

    return 0;
}
