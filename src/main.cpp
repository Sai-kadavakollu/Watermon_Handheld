#include <Arduino.h>
#include "CApplication.h"

// #define SERIAL_DEBUG
#ifdef SERIAL_DEBUG
#define debugPrint(...) Serial.print(__VA_ARGS__)
#define debugPrintln(...) Serial.println(__VA_ARGS__)
#define debugPrintf(...) Serial.printf(__VA_ARGS__)
#define debugPrintlnf(...) Serial.println(F(__VA_ARGS__))
#else
#define debugPrint(...)    // blank line
#define debugPrintln(...)  // blank line
#define debugPrintf(...)   // blank line
#define debugPrintlnf(...) // blank line
#endif

TaskHandle_t frameHandlingTaskHandler;
TaskHandle_t applicationTaskHandler;
TaskHandle_t commandParseTaskHandler;
TaskHandle_t OtaTaskHandler;
TaskHandle_t SmartConfigHandler;

cApplication App;

/************************
 * Task 1
 *************************/
void Task1code(void *pvParameters)
{
  for (;;)
  {
    App.frameHandlingTask();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/************************
 * Task2
 *************************/
void Task2code(void *pvParameters)
{
  for (;;)
  {
    App.applicationTask();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void printTaskStacks()
{
  Serial.println("---- Task Stack Usage ----");
  Serial.printf("Frame       : %u words free\n", uxTaskGetStackHighWaterMark(frameHandlingTaskHandler));
  Serial.printf("App         : %u words free\n", uxTaskGetStackHighWaterMark(applicationTaskHandler));
  Serial.printf("Modbus      : %u words free\n", uxTaskGetStackHighWaterMark(commandParseTaskHandler));
  Serial.printf("OTA         : %u words free\n", uxTaskGetStackHighWaterMark(OtaTaskHandler));
  Serial.printf("SmartConfig : %u words free\n", uxTaskGetStackHighWaterMark(SmartConfigHandler));
  Serial.println("--------------------------");
}

/************************
 * Task3
 *************************/
void Task3code(void *pvParameters)
{
  for (;;)
  {
    App.commandParseTask();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

/************************************************
 * Task to handle OTA updates
 ************************************************/
void Task4code(void *pvParameter)
{
  for (;;)
  {
    App.fotaTask();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
/************************************************
 * Task to handle smart config
 ************************************************/
void Task5code(void *pvParameters)
{
  for (;;)
  {
    App.GpsTask();
    App.SmartConfigTask();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

/*******************************
 *  Function that create tasks
 ********************************/
void CreateTasks(int Value)
{
  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
      Task1code,                    /* Task function. */
      "Frame",                      /* name of task. */
      18000,                        /* Stack size of task */
      NULL,                         /* parameter of the task */
      2,                            /* priority of the task */
      &frameHandlingTaskHandler,                       /* Task handle to keep track of created task */
      CONFIG_ARDUINO_RUNNING_CORE); /* pin task to core 1 */
  delay(500);

  // create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
      Task2code,                    /* Task function. */
      "App",                        /* name of task. */
      20000,                        /* Stack size of task */
      NULL,                         /* parameter of the task */
      3,                            /* priority of the task */
      &applicationTaskHandler,                       /* Task handle to keep track of created task */
      CONFIG_ARDUINO_RUNNING_CORE); /* pin task to core 1 */
  delay(500);

  // create a task that will be executed in the Task3code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
      Task3code, /* Task function. */
      "Modbus",  /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &commandParseTaskHandler,    /* Task handle to keep track of created task */
      0);        /* pin task to core 0 */
  delay(500);

  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
      Task4code,       /* Task function. */
      "OTA",           /* name of task. */
      15000,           /* Stack size of task */
      NULL,            /* parameter of the task */
      4,               /* priority of the task */
      &OtaTaskHandler, /* Task handle to keep track of created task */
      CONFIG_ARDUINO_RUNNING_CORE);              /* pin task to core 1 */
  delay(500);
  // create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
      Task5code,           /* Task function. */
      "SmartConfig",       /* name of task. */
      5000,                /* Stack size of task */
      NULL,                /* parameter of the task */
      5,                   /* priority of the task */
      &SmartConfigHandler, /* Task handle to keep track of created task */
      0);                  /* pin task to core 1 */
  delay(500);
}

/************************
 * Setup initialization
 *************************/
void setup()
{
  // put your setup code here, to run once:
  debugPrintln("=================================Code begin=================================");
  int retValue = App.appInit();
  if (retValue)
  {
    CreateTasks(retValue);
    App.AppWatchdogInit(&frameHandlingTaskHandler, &applicationTaskHandler, &commandParseTaskHandler, &OtaTaskHandler, &SmartConfigHandler);
    debugPrintln("Total 5 Watchdog Init.....");
  }
  else
  {
    //Safe Mode
  }
}

/************************
 * loop
 *************************/
void loop()
{
}