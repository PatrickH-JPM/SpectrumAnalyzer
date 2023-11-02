/********************************************************************************************************************************
*
*  Project :          21 Bands Spectrum Analyzer
*  Purpose :          PFE / PES (Projet de Fin d'Etude / Projet of End Of Study)
*  Target Platform :  Arduino Due or (Arduino Mega2560 or Mega2560 PRO MINI)
*  
*  Version :          2.00
*  Hardware setup :   https://github.com/...
*  
*  Author :           Patrick HANNA (special thx to Platinum, Jarzebski, Mark Donners )  
*  School :           Polytech Tours - ISIE 5A
*  github :           https://github.com/PatrickH-JPM
*
*
*  IMPORTANT :
*  Spectrum analyses done with analog chips MSGEQ7
*  The library Si5351mcu is being utilized for programming masterclock IC frequencies.
*  Special thanks to Pavel Milanes for his outstanding work. https://github.com/pavelmc/Si5351mcu (unused now)
*  It is not possible to run modified code on a UNO,NANO or PRO MINI. due to memory limitations.
*  Analog reading of MSGEQ7 IC1, IC2 and IC3 use pin A0, A1 and A2.
*  Clock pin from MSGEQ7 IC1, IC2 and IC3 to Si5351mcu board clock 0, 1 and 2
*  Si5351mcu SCL and SDA use pin 20 and 21
*  See the Schematic Diagram for more info (...)
*
******************************************************************************************************************************** 
*  Version History
*  1.0  First working version adapted from Platinum's free basic version All credits for this great idea go to him.
*  2.0  Adaptation du code pour le passer en 21 bands et ajout des quelques fonctions basiques
			- Changement de lib si5351mcu to Adafruit_SI5351 car celle de pavel ne permet que 2 sorties activées en simultanné
			- Ajout des commentaires
			- Passage en lecture de 3 MSGEQ7
*
*  ------
*  Latest Version
*  2.0
*
********************************************************************************************************************************
*  ------ Tests à mener ------
*  Verif la conso des LED de 20 à 255 puis bloquer la luminosité max
*  Tester si la fréquence de décalaga avec les resitance condo est bonne
*
*
*  ------ Modifs à apporter ------
*  Voir si le générateur de pulse est obligatoire ou inutile
*  Nettoyer la fonction setup
*  Debuger le top point pour la dernière colonne
*  Ajout des potars
*
********************************************************************************************************************************

/******************** Hypotheses *************************/
//
// peak delais peut être le temps que le peak reste en haut
// delais peut être le temps que le peak met à redescendre
// calculer résolution et précision du due
//
/*********************************************************/




#include <Adafruit_NeoPixel.h>
/*
#include <si5351mcu.h>    //Si5351mcu library
Si5351mcu Si;             //Si5351mcu Board
*/
#include <Adafruit_SI5351.h>
Adafruit_SI5351 clockgen = Adafruit_SI5351();

#define PULSE_PIN     13  //inutilisé ?
#define NOISE         50  //inutilisé ?

#define ROWS          20  //num of row MAX=20
#define COLUMNS       21  //num of column

#define DATA_PIN      9   //led data pin
#define STROBE_PIN    6   //MSGEQ7 strobe pin
#define RESET_PIN     7   //MSGEQ7 reset pin

#define NUMPIXELS    ROWS * COLUMNS

struct Point {
  char x, y;
  char  r, g, b;
  bool active;
};

struct TopPoint {
  int position;
  int peakpause;
};

Point spectrum[ROWS][COLUMNS];
TopPoint peakhold[COLUMNS];
int spectrumValue[COLUMNS];
long int counter = 0;
//--------------------------//
int long pwmpulse = 0;
bool toggle = false;
//--------------------------//
int long time_change = 0;
int effect = 0;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

void setup()
{
  Serial.begin(9600);
  /*
  Si.init(25000000L);

  Si.setFreq(0, 104570);
  Si.setFreq(1, 166280);
  Si.setFreq(2, 189785);

  Si.setPower(0, SIOUT_8mA);
  Si.setPower(1, SIOUT_8mA);
  Si.setPower(2, SIOUT_8mA);

  Si.enable(0);
  Si.enable(1);
  Si.enable(2);
  */
  if (clockgen.begin() != ERROR_NONE)
  {
    Serial.print("Ooops, no Si5351 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  Serial.println("Init clk OK !");

  clockgen.setupPLL(SI5351_PLL_A, 24, 2, 3);

  clockgen.setupMultisynth(0, SI5351_PLL_A, 92, 0, 1);
  clockgen.setupRdiv(0, SI5351_R_DIV_64);

  clockgen.setupMultisynth(1, SI5351_PLL_A, 64, 0, 1);
  clockgen.setupRdiv(1, SI5351_R_DIV_64);
  
  clockgen.setupMultisynth(2, SI5351_PLL_A, 48, 0, 1);    //52 = 180    48
  clockgen.setupRdiv(2, SI5351_R_DIV_64);

  clockgen.enableOutputs(true);

  pinMode      (STROBE_PIN,    OUTPUT);
  pinMode      (RESET_PIN,     OUTPUT);
  pinMode      (DATA_PIN,      OUTPUT);
  pinMode      (PULSE_PIN,     OUTPUT);  //inutilisé ?

  /******* utilité ??? ********/
  digitalWrite(PULSE_PIN, HIGH);
  delay(100);
  digitalWrite(PULSE_PIN, LOW);
  delay(100);
  digitalWrite(PULSE_PIN, HIGH);
  delay(100);
  digitalWrite(PULSE_PIN, LOW);
  delay(100);
  digitalWrite(PULSE_PIN, HIGH);
  delay(100);
  /*****************************/

  pixels.setBrightness(20);               //set Brightness
  
  pixels.begin();
  pixels.show();

  pinMode      (STROBE_PIN, OUTPUT);
  pinMode      (RESET_PIN,  OUTPUT);

  digitalWrite (RESET_PIN,  LOW);         // reset bas
  digitalWrite (STROBE_PIN, LOW);         // strobe bas
  delay        (1);
  digitalWrite (RESET_PIN,  HIGH);        // reset haut
  delay        (1);
  digitalWrite (RESET_PIN,  LOW);         // reset bad
  digitalWrite (STROBE_PIN, HIGH);        // strobe haut
  delay        (1);
  // séquence de démmarage \\.
}

void loop()
{
  counter++;
  clearspectrum();
  pixels.setBrightness(20);               //set Brightness // ajouter variable
  

  // *** sert à rien ? *** //
  if (millis() - pwmpulse > 3000) {
    toggle = !toggle;
    digitalWrite(PULSE_PIN, toggle);
    pwmpulse = millis();
  }
  // ******************** //

  digitalWrite(RESET_PIN, HIGH);
  delayMicroseconds(3000);
  digitalWrite(RESET_PIN, LOW);

  //*** lecture valeurs analogiques des MSGEQ7 ***//
  for (int i = 0; i < COLUMNS; i++) {

    digitalWrite(STROBE_PIN, LOW);
    
    delayMicroseconds(1000);                                          //1ms
    
    
    spectrumValue[i] = analogRead(0);                                 //lecture 1er MSGEQ7
    if (spectrumValue[i] < 120)spectrumValue[i] = 0;                  //niv minimum ?
    spectrumValue[i] = constrain(spectrumValue[i], 0, 1023);
    spectrumValue[i] = map(spectrumValue[i], 0, 1023, 0, ROWS); i++;  //passage au valeurs du MSGEQ7 2
    
    
    spectrumValue[i] = analogRead(1);                                 //lecture 2eme MSGEQ7
    if (spectrumValue[i] < 120)spectrumValue[i] = 0;
    spectrumValue[i] = constrain(spectrumValue[i], 0, 1023); 
    spectrumValue[i] = map(spectrumValue[i], 0, 1023, 0, ROWS); i++;  //passage au valeurs du MSGEQ7 3


    spectrumValue[i] = analogRead(2);                                 //lecture 3eme MSGEQ7
    if (spectrumValue[i] < 120)spectrumValue[i] = 0;
    spectrumValue[i] = constrain(spectrumValue[i], 0, 1023);
    spectrumValue[i] = map(spectrumValue[i], 0, 1023, 0, ROWS);



    digitalWrite(STROBE_PIN, HIGH);                                   //changement de freq/sortie du MSGEQ7 (freq suivante)
  }
  /*************************************************/

  //allumage des LED en commencant par le bas
  for (int j = 0; j < COLUMNS; j++) {
    for (int i = 0; i < spectrumValue[j]; i++){
      spectrum[i][COLUMNS - 1 - j].active = 1;     // active la LED dans la position calculée
      //ajustent la couleur de la LED (dans ce cas, vert)
      spectrum[i][COLUMNS - 1 - j].r = 0;          //COLUMN Color red
      spectrum[i][COLUMNS - 1 - j].g = 255;        //COLUMN Color green
      spectrum[i][COLUMNS - 1 - j].b = 0;          //COLUMN Color blue
    }
  
  //allumage des peaks
    if (spectrumValue[j] - 1 > peakhold[j].position){
    // Si le spectre actuel est plus élevé que le pic précédemment enregistré, le nouveau pic est mis à jour.
  
    // Les lignes suivantes éteignent la LED du pic précédent (en mettant ses composants couleur à 0).
      spectrum[spectrumValue[j] - 1][COLUMNS - 1 - j].r = 0;
      spectrum[spectrumValue[j] - 1][COLUMNS - 1 - j].g = 0;
      spectrum[spectrumValue[j] - 1][COLUMNS - 1 - j].b = 0;
      
      // Mettre à jour la position du nouveau pic.
      peakhold[j].position = spectrumValue[j] - 1;
      
      // Réinitialiser le délai pendant lequel le pic est maintenu (peakpause) avant de commencer à "retomber".
      peakhold[j].peakpause = 1; //set peakpause
    }

    else{
      // Si le spectre actuel n'est pas plus élevé que le pic enregistré,
      // le pic précédent reste allumé et ne change pas de position.

      // Activer le pic (LED) à sa position précédente.
      spectrum[peakhold[j].position][COLUMNS - 1 - j].active = 1;
      // couleur active : jaune (rouge + vert)
      spectrum[peakhold[j].position][COLUMNS - 1 - j].r = 255;  //Peak Color red
      spectrum[peakhold[j].position][COLUMNS - 1 - j].g = 255;  //Peak Color green
      spectrum[peakhold[j].position][COLUMNS - 1 - j].b = 0;    //Peak Color blue
    }
  }

    flushMatrix();

    if (counter % 3 == 0)
      topSinking(); //peak delay
}



void topSinking(){
  for (int j = 0; j < ROWS; j++)     // Boucle à travers touts les étages de la matrice d'affichage
  {
    // Si la position actuelle du pic n'est pas au niveau le plus bas (plus grand que 0) et qu'il n'est pas censé 
    // s'arrêter (peakpause est 0 ou moins), alors diminue la position du pic de 1. Cela "abaisse" le pic dans 
    // l'affichage visuel.
    if (peakhold[j].position > 0 && peakhold[j].peakpause <= 0) 
      peakhold[j].position--;

    // Si "peakpause" est supérieur à 0, cela signifie que le pic est dans un état où il ne devrait pas descendre.
    // Donc, on décrémente "peakpause" de 1 pour continuer le compte à rebours jusqu'à ce qu'il puisse à nouveau 
    // commencer à descendre.
    else if (peakhold[j].peakpause > 0) 
      peakhold[j].peakpause--;
  }
}


//éteint toute la matrice
void clearspectrum(){
  for (int i = 0; i < ROWS; i++)
  {
    for (int j = 0; j < COLUMNS; j++)
    {
      spectrum[i][j].active = false;
    }
  }
}


void flushMatrix(){
  for (int j = 0; j < COLUMNS; j++)
  {
    if ( j % 2 != 0)      // Vérifie si l'index de la colonne est impair. Cela est fait pour créer un effet zébré ou pour compenser pour un certain arrangement physique des LEDs.
    {
      for (int i = 0; i < ROWS; i++)    // Si la colonne est impaire, itère à travers toutes les lignes de la matrice.
      {
        if (spectrum[ROWS - 1 - i][j].active)     // Vérifie si le LED à la position actuelle est actif.
        {
          // Si le LED est actif, définit sa couleur. Notez que les coordonnées sont inversées pour les colonnes impaires.
          // La méthode setPixelColor prend l'index du LED et les valeurs RGB pour la couleur.
          pixels.setPixelColor(j * ROWS + i, pixels.Color(
                                 spectrum[ROWS - 1 - i][j].r,
                                 spectrum[ROWS - 1 - i][j].g,
                                 spectrum[ROWS - 1 - i][j].b));
        }
        else
        {
          pixels.setPixelColor( j * ROWS + i, 0, 0, 0);       // Si le LED n'est pas actif, définit sa couleur à noir, ce qui l'éteint en pratique.
        }
      }
    }
    else   // Si la colonne est paire.
    {
      for (int i = 0; i < ROWS; i++)    // Vérifie si le LED à la position actuelle est actif.
      {
        if (spectrum[i][j].active)
        {
          // Si le LED est actif, définit sa couleur. La coordination est standard pour les colonnes paires.
          pixels.setPixelColor(j * ROWS + i, pixels.Color(    
                                 spectrum[i][j].r,
                                 spectrum[i][j].g,
                                 spectrum[i][j].b));
        }
        else
        {
          pixels.setPixelColor( j * ROWS + i, 0, 0, 0);   // Si le LED n'est pas actif, définit sa couleur à noir, ce qui l'éteint en pratique.
        }
      }
    }
  }

  pixels.show();    // Après avoir parcouru et mis à jour toutes les LEDs, la méthode 'show' est appelée pour appliquer les changements sur la matrice de LEDs.

}
