
/* DC Motor Regulator */

/* Hardware: Arduino Nano */

#define PIN_PWMOUT 6
#define PIN_CURRENTFEEDBACK A0 /* ADC0 is the voltage over the motor shunt */
#define PIN_UMOTFEEDBACK A1 /* ADC1 is the motor voltage */
#define PIN_UPEDAL A2 /* ADC2 is the pedal voltage */

int16_t pwm_iPart;
int16_t pwm_pPart;
int16_t pwm;
int16_t pwmOut;
int32_t TMotor_mC=30000; /* starting the thermal model at 30°C */
int32_t pDissipated_mW;
int32_t pRadiated_mW;
int32_t pHeating_mW;
uint16_t coolDownTimer;
uint8_t rattling;
uint16_t loopDivider;
int32_t powerIntegral;

int16_t uGenOld;

int16_t uGenFilt;

void setup()
{
  pinMode(PIN_PWMOUT, OUTPUT);
  analogWrite(PIN_PWMOUT, 128); /* 0 to 255 */
  analogReference(INTERNAL); /* 1.1 volts on the ATmega168 */
  Serial.begin(115200);
  Serial.println("Started");
  pwm = 10; /* 10 of 255 as starting value */
}

#define U_SUPPLY_mV 12000


void loop()
{
  int32_t tmp;
  int16_t uMotor_mV;
  int16_t uShunt_mV;
  int16_t uPedal_mV;
  int16_t pedalway_percent;
  int16_t uRInternal_mV;
  int16_t uGenerated_mV;
  int16_t uDeviation_mV;  
  int16_t uGenTarget_mV;

  tmp = analogRead(PIN_UPEDAL);
  tmp *= 1100; /* reference voltage is 1.1V */
  tmp /= 1023; /* max ADC value */
  uPedal_mV = tmp;
  
  tmp = (695-uPedal_mV);
  tmp *= 150;
  tmp /= 100;
  if (tmp<0) tmp=0;
  if (tmp>100) tmp=100;  
  pedalway_percent = tmp;
  
  tmp = analogRead(PIN_CURRENTFEEDBACK);
  tmp *= 1100; /* reference voltage is 1.1V */
  tmp /= 1023; /* max ADC value */
  uShunt_mV = tmp;
  
  tmp = analogRead(PIN_UMOTFEEDBACK);
  tmp *= 1100; /* reference voltage is 1.1V */
  tmp /= 1023; /* max ADC value */
  tmp *= (12+100); /* upper resistor is 100k */
  tmp /= 12; /* lower resistor is 12k */
  uMotor_mV = tmp;

  /* 1000 and 900: instable under load */
  /* 500: too less force under load */
  /* 800: ok */
  #define RInternal_per_RShunt_percent 830 /* shunt has 0.2 ohm, Ri has 2 ohm. */
  tmp =   uShunt_mV;
  tmp *= RInternal_per_RShunt_percent;
  tmp /=100;  
  uRInternal_mV = tmp; 
  uGenerated_mV = uMotor_mV - uRInternal_mV;

  /* calculate dissipated power over Ri. P=U*U/R */
  tmp = uRInternal_mV;
  tmp *= uRInternal_mV;
  tmp /= 2; /* Ri 2 ohm */
  tmp /=1000; /* microwatt to milliwatt */
  pDissipated_mW = tmp;
  pRadiated_mW = (TMotor_mC-22000)/1000*500; /* assumed: 500mW/K, and 22°C environment */
  pHeating_mW =  pDissipated_mW -  pRadiated_mW; /* difference between produced and radiated energy leads to heating */
  TMotor_mC = TMotor_mC + pHeating_mW/2000; /* motors temperature change in milli°C, caused by power and thermal capacity */

  if (TMotor_mC>50000) {   /* temperature above 50°C */ 
     coolDownTimer = 100; /* 1 second pause */ 
  }

  uGenFilt = (uGenOld+uGenerated_mV)/2;
  uGenOld = uGenerated_mV;

  uGenTarget_mV = 1200; /* Target generator voltage */
  uGenTarget_mV = pedalway_percent * 12; /* 1200mV for 100% pedal */

  uDeviation_mV = uGenFilt - uGenTarget_mV;
  /* Integral part */
  if (uDeviation_mV>200) {
    if (pwm_iPart>0) pwm_iPart-=4;
    if ((uDeviation_mV>1000) && (pwm_iPart>20)) { pwm_iPart-=10; }
  }
  if (uDeviation_mV<-500) {
    if (pwm_iPart<150) pwm_iPart+=7;    
  }
  if (coolDownTimer!=0) {
    pwm_iPart=0;    
  }
  
  pwm_pPart = 15 - uDeviation_mV/10;
  pwm = pwm_iPart + pwm_pPart;
  //pwm=6;

  if (coolDownTimer>0) {
      pwm = 3;
      coolDownTimer--;
  }
  if (pedalway_percent<5) {
    pwm=0;
  }
  if (pwm<0) pwm = 0;
  if (pwm>255) pwm = 255;
  if (pwm<10) {
    pwmOut=pwm*2;
    rattling = 1;
  } else {
    pwmOut=pwm;
    rattling=0;
  }
  loopDivider++; 
  if ((loopDivider%2)==0) {
    if (rattling) {
      analogWrite(PIN_PWMOUT, 0); /* 0 to 255 */
    } else {
      analogWrite(PIN_PWMOUT, pwmOut); /* 0 to 255 */      
    }
  } else {
    analogWrite(PIN_PWMOUT, pwmOut); /* 0 to 255 */    
  } 

  //Serial.println(String(uPedal_mV) + " " + String(pedalway_percent));
  if ((loopDivider%20)==0) {  
    Serial.println("uTarget=" + String(uGenTarget_mV) + " uShunt=" + String(uShunt_mV) + " umot=" + String(uMotor_mV) + " uGen=" + String(uGenerated_mV) +
                 " uGenFil=" + String(uGenFilt) + " devi = " + String(uDeviation_mV) + " pwm=" + String(pwm) +
                 " pDis=" + String(pDissipated_mW) + " temp=" + String(TMotor_mC/1000) + " pRad=" + String(pRadiated_mW));
                   
  }
  delay(10);
}