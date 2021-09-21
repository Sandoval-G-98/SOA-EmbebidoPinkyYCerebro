#include <IRremote.h>
#include <Adafruit_NeoPixel.h>

// Habilitacion de debug para la impresion por el puerto serial ...
//----------------------------------------------
#define SERIAL_DEBUG_ENABLED 0

#if SERIAL_DEBUG_ENABLED
#define DebugPrint(str)      \
    {                        \
        Serial.println(str); \
    }
#else
#define DebugPrint(str)
#endif

#define DebugPrintEstado(estado,evento)\
      {\
        String est = estado;\
        String evt = evento;\
        String str;\
        str = "-----------------------------------------------------";\
        DebugPrint(str);\
        str = "EST-> [" + est + "]: ";\
        DebugPrint(str);\
        str = "EVT-> [" + evt + "].";\
        DebugPrint(str);\
        str = "-----------------------------------------------------";\
        DebugPrint(str);\
      }
//----------------------------------------------

// Declaracion de pines
const int recv_pin = 2;
const int led_green = 3;
const int led_red = 4;
const int vibration_motor = 5;
const int piezo = 6;
const int force_sensor = A0;
const int neo_ring = 8;
const int ambient_light_sensor = A1;

// Temporizadores
#define TMP_EVENTS 50
unsigned long previous_time;
unsigned long actual_time;

// Puerto serial
#define SERIAL_PORT 9600
// IRS receptor
IRrecv irrecv(recv_pin);
decode_results results;
#define IR_SENSOR_ACTIVATE 0xFD00FF //0xFD00FF
#define IR_SENSOR_DESACTIVE 0xFD40BF
#define MIN_VALUE_FORCE_SENSOR 0
#define MIN_VALUE_NIGHT 700
#define NUM_LASER 12
#define _initLASER 1
#define YELLOW 255
#define BLUE 255
#define WHITE 0
#define LED_0 0
#define LED_1 1
#define LED_2 2
#define LED_3 3
#define LED_4 4
#define LED_5 5
#define LED_6 6
#define LED_7 7
#define LED_8 8
#define LED_9 9
#define LED_10 10
#define LED_11 11
//
Adafruit_NeoPixel laser(NUM_LASER, neo_ring, NEO_GRB + NEO_KHZ800);
// Argumento 1 = Número de pixeles encadenados
// Argumento 2 = Número del pin de Arduino utilizado con pin de datos
// Argumento 3 = Banderas de tipo de pixel:
// NEO_KHZ800    800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
// NEO_GRB       Pixels are wired for GRB bitstream (most NeoPixel products)

// Estados y eventos
#define MAX_STATES 5
#define MAX_EVENTS 7
enum states
{
    ST_OFF,
    ST_INIT,
    ST_IN_DAY,
    ST_AT_NIGHT,
    ST_IN_BED
} current_state;
String states_s[] = {"ST_OFF", "ST_INIT", "ST_IN_DAY", "ST_AT_NIGHT", "ST_IN_BED"};

enum events
{
    EV_DUMMY,
    EV_TURN_ON,
    EV_TURN_OFF,
    EV_IS_DAY,
    EV_AT_NIGHT,
    EV_LYING_DOWN,
    EV_GET_UP
} new_event;
String events_s[] = {"EV_DUMMY", "EV_TURN_ON", "EV_TURN_OFF", "EV_IS_DAY", "EV_AT_NIGHT", "EV_LYING_DOWN", "EV_GET_UP"};


/// Matriz de estados
typedef void (*transition)();
transition state_table[MAX_STATES][MAX_EVENTS] =
    {
        {none,  turn_on,     error,          error,          error,          error,            error},    // state ST_OFF
        {none,  error,       turn_off_1,     got_day,        got_dark_1,     lying_down,       get_up},   // state ST_INIT
        {none,  error,       turn_off_1,     got_day,        got_dark_1,     lying_down,       get_up},   // state ST_IN_DAY
        {none,  error,       turn_off_1,     got_day ,       got_dark_1,     lying_down,       get_up},   // state ST_AT_NIGHT
        {none,  error,       turn_off_2,     got_day,        got_dark_2,     lying_down,       get_up}    // state ST_IN_BED
    //EV_DUMMY, EV_TURN_ON  , EV_TURN_OFF  , EV_IS_DAY      , EV_AT_NIGHT ,  EV_LYING_DOWN  , EV_GET_UP
};

/// El error nos indica 
void error()
{
    DebugPrintEstado(states_s[current_state], events_s[new_event]);
}

void none()
{
}

void update_led_red(int state)
{
    //Funcion que setea el estado del led rojo segun el estado que le llegue.
    digitalWrite(led_red, state);
}

void update_led_green(int state)
{
    //Funcion que setea el estado del verde rojo segun el estado que le llegue.
    digitalWrite(led_green, state);
}

void got_day(){
    //Sensor de ambiente detecta que es de dia. Seteamos el estado y apagamos los actuadores.
    turn_off_all();
    current_state = ST_IN_DAY;
}

void turn_on()
{
    //Funcion que setea el sistema embebido en su estado inicial.
    //Seteando los leds correspondientes al encender.
    update_led_red(LOW);
    update_led_green(HIGH);
    current_state = ST_INIT;
}

void got_dark_1()
{
    // Escuchar el sensor Force
    current_state = ST_AT_NIGHT;
}

void got_dark_2()
{
    //Si la persona ya se encontraba acostado antes que se haga de noche
    //esta funcion escucha al sensor de ambiente, y si oscurece, se setean los actuadores.
    turn_on_all();
    current_state = ST_IN_BED;
}

void turn_off_1()
{
    //No hay sensores activos, por lo tanto solo apagamos el sistema.
    update_led_red(HIGH);
    update_led_green(LOW);
    current_state = ST_OFF;
}

void turn_off_2()
{
    //Hay sensores activos, por lo tanto, antes de apagar el sistema seteamos los valores de los actuadores.
    turn_off_all();
    update_led_red(HIGH);
    update_led_green(LOW);
    current_state = ST_OFF;
}

void lying_down()
{
    current_state = ST_IN_BED; //ST_AT_NIGHT
}

void get_up()
{
    //Deberia de apagarse todos los actuadores (PIEZO, RING, VIBRADOR)
    turn_off_all();
    current_state = ST_INIT;
}

void turn_on_all ()
{
    // Encendemos todos los actuadores
    turn_on_neo_ring();
    turn_on_vibration_motors();
    turn_on_piezo();
}

void turn_off_all ()
{
    // Apágamos todos los actuadores
    turn_off_neo_ring();
    turn_off_vibration_motors();
    turn_off_piezo();
}


/// Funciones de encendido/apagado de actuadores 
void turn_on_neo_ring()
{
    laser.clear();
    laser.setPixelColor(LED_0, laser.Color(YELLOW, YELLOW, WHITE));
    laser.setPixelColor(LED_1, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_2, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_3, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_4, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_5, laser.Color(YELLOW, YELLOW, WHITE));
    laser.setPixelColor(LED_6, laser.Color(YELLOW, YELLOW, WHITE));
    laser.setPixelColor(LED_7, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_8, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_9, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_10, laser.Color(WHITE, WHITE, BLUE));
    laser.setPixelColor(LED_11, laser.Color(YELLOW, YELLOW, WHITE));
    laser.show();
}
void turn_off_neo_ring()
{   
    laser.clear();
    laser.setPixelColor(LED_0, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_1, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_2, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_3, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_4, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_5, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_6, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_7, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_8, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_9, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_10, laser.Color(WHITE, WHITE, WHITE));
    laser.setPixelColor(LED_11, laser.Color(WHITE, WHITE, WHITE));
    laser.show();
}

void turn_on_vibration_motors(){
    digitalWrite(vibration_motor,1);
}
void turn_off_vibration_motors(){
    digitalWrite(vibration_motor,0);
}
void turn_on_piezo(){
    digitalWrite(piezo,HIGH);
}
void turn_off_piezo(){
    digitalWrite(piezo,LOW);
}
//////////----------------------------------

/// Funciones de lectura de sensores 
int read_force_sensor(){
  return analogRead(force_sensor);
}
int read_ambient_light_sensor(){
  return analogRead(ambient_light_sensor);
}
////////----------------------------------

/// Funciones qeu verifican los sensores y setean estado
void check_force_sensor(){
    
    int value = read_force_sensor();
    if(value > MIN_VALUE_FORCE_SENSOR){
      new_event = EV_LYING_DOWN;
      return;
    } 

    new_event = EV_GET_UP;
}


void check_ambient_ligh_sensor(){

  int value = read_ambient_light_sensor(); 
    if(value >= MIN_VALUE_NIGHT){
        new_event = EV_AT_NIGHT;
        return;
    } 

    new_event = EV_IS_DAY;
}

bool check_press_ir_sensor()
{
    //decode_results results;
    bool isTurn = false;
    unsigned int value = 0;
    if (irrecv.decode(&results)) //irrecv.decode(&results) returns true if anything is recieved, and stores info in varible results
    {
        irrecv.resume();
        // value = results.value; //Get the value of results as an unsigned int, so we can use switch case
        if (results.value == IR_SENSOR_ACTIVATE) //boton rojo
        {
  			
          new_event = EV_TURN_ON;
            isTurn = true;
        }
        else if (results.value == IR_SENSOR_DESACTIVE) //boton func/off
        {
            new_event = EV_TURN_OFF;
            isTurn = true;
        }
    }
	
    return isTurn;
}
////////----------------------------------

int counter = 0;
/// Obtencion de eventos
void get_new_event()
{   
    if(check_press_ir_sensor()){
        return;
    } 

    actual_time = millis();
    if ((actual_time - previous_time) > TMP_EVENTS)
    {
        if(counter%2 == 0)
            check_ambient_ligh_sensor();
        else 
            check_force_sensor();
        counter++;
        previous_time = actual_time;
        return;
    }
	
    new_event = EV_DUMMY;
}
////////----------------------------------

/// Lanzamos los eventos que se obtuvieron del get_new_event
void gargola_state_machine()
{
    get_new_event();

    if ((new_event >= 0) && (new_event < MAX_EVENTS) && (current_state >= 0) && (current_state < MAX_STATES))
    {
        state_table[current_state][new_event]();
    }

    // Consumo el evento...
    new_event = EV_DUMMY;
}
////////----------------------------------


/// Inicializador
void _init(){
    pinMode(led_red, OUTPUT);
    pinMode(led_green, OUTPUT);
    pinMode(vibration_motor, OUTPUT); 
    pinMode(piezo, OUTPUT);
    update_led_red(HIGH);
    Serial.begin(SERIAL_PORT);
    Serial.println("-----------------------------------------");
    Serial.println("Comienzo de sistema");
    irrecv.enableIRIn();
    Serial.println("Esperando inicio senal de inicio...");
    Serial.println("-----------------------------------------");
    previous_time = millis();
    current_state = ST_OFF;
}

/// Funciones arduino
void setup()
{
    _init();
}

void loop()
{
    gargola_state_machine();
}
////////----------------------------------