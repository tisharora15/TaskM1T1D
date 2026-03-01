-#include <avr/interrupt.h>   // Needed for hardware interrupts

#define BUTTON_ARM     8     // Arm button (PCI)
#define BUTTON_DISARM  9     // Disarm button (PCI)
#define PIR_PIN        2     // PIR sensor (External interrupt)
#define TRIG_PIN       4     // Ultrasonic trigger
#define ECHO_PIN       5     // Ultrasonic echo
#define LED_PIN        13    // Alarm LED

volatile bool systemArmed = false;     // True when system is armed
volatile bool pirMotion = false;       // True when motion detected
volatile bool timerFlag = false;       // True when timer interrupt occurs
volatile bool armRequest = false;      // Arm request from button
volatile bool disarmRequest = false;   // Disarm request from button

volatile bool pirTriggered = false;    
volatile bool pciTriggered = false;    
volatile bool timerTriggered = false;  

long distanceCM = 0;                   // Stores ultrasonic distance
unsigned long lastDebounceTime = 0;    // Stores last button press time
const unsigned long debounceDelay = 200; // 200ms debounce time

void setupTimer1();
void readUltrasonic();
void processLogic();
void updateOutput();

void setup() {

  Serial.begin(9600);  // Start serial monitor

  // Set pin modes
  pinMode(BUTTON_ARM, INPUT_PULLUP);   
  pinMode(BUTTON_DISARM, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Enable Pin Change Interrupt group for D8–D13
  PCICR |= (1 << PCIE0);

  // Enable interrupt for D8 and D9
  PCMSK0 |= (1 << PCINT0);
  PCMSK0 |= (1 << PCINT1);

  // Attach external interrupt for PIR (RISING edge)
  attachInterrupt(digitalPinToInterrupt(PIR_PIN), pirISR, RISING);

  setupTimer1();  // Configure timer
}

void loop() {

  // Show which interrupt was triggered
  if (pirTriggered) {
    pirTriggered = false;
    Serial.println("PIR Interrupt Triggered");
  }

  if (pciTriggered) {
    pciTriggered = false;
    Serial.println("PCI Interrupt Triggered");
  }

  if (timerTriggered) {
    timerTriggered = false;
    Serial.println("Timer Interrupt Triggered");
  }

  // Handle ARM button with debounce
  if (armRequest && millis() - lastDebounceTime > debounceDelay) {
    armRequest = false;
    systemArmed = true;
    lastDebounceTime = millis();
    Serial.println("System ARMED");
  }

  // Handle DISARM button with debounce
  if (disarmRequest && millis() - lastDebounceTime > debounceDelay) {
    disarmRequest = false;
    systemArmed = false;
    pirMotion = false;   // Reset motion when disarmed
    lastDebounceTime = millis();
    Serial.println("System DISARMED");
  }

  if (timerFlag) {
    timerFlag = false;

    readUltrasonic();   // Measure distance

    Serial.print("Armed: ");
    Serial.print(systemArmed);
    Serial.print(" | Motion: ");
    Serial.print(pirMotion);
    Serial.print(" | Distance: ");
    Serial.println(distanceCM);
  }

  processLogic();   // Handle system logic
  updateOutput();   // Control LED
}

void pirISR() {
  pirMotion = true;       // Motion detected
  pirTriggered = true;    // Debug flag
}

ISR(PCINT0_vect) {

  pciTriggered = true;    // Debug flag

  if (digitalRead(BUTTON_ARM) == LOW) {
    armRequest = true;    // Request arm
  }

  if (digitalRead(BUTTON_DISARM) == LOW) {
    disarmRequest = true; // Request disarm
  }
}

ISR(TIMER1_COMPA_vect) {
  timerFlag = true;        // Timer event occurred
  timerTriggered = true;   
}

void setupTimer1() {

  noInterrupts();     // Disable interrupts during setup

  TCCR1A = 0;         // Clear register A
  TCCR1B = 0;         // Clear register B

  OCR1A = 31249;      // 2-second interval

  TCCR1B |= (1 << WGM12);               // CTC mode
  TCCR1B |= (1 << CS12) | (1 << CS10);  // Prescaler 1024

  TIMSK1 |= (1 << OCIE1A);  // Enable compare interrupt

  interrupts();      // Enable interrupts
}

void readUltrasonic() {

  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 20000);  // Measure echo time
  distanceCM = duration * 0.034 / 2;               // Convert to cm
}

void processLogic() {
  if (!systemArmed) {
    pirMotion = false;   // Clear motion if system is off
  }
}

void updateOutput() {

  // LED ON only if all conditions true
  if (systemArmed && pirMotion && distanceCM > 0 && distanceCM < 20) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}
