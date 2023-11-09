// Infos prog
/********************************************************************************************************************************
  *
  *  Project :          21 Bands Spectrum Analyzer
  *  Purpose :          PFE / PES (Projet de Fin d'Etude / Projet of End Of Study)
  *  Target Platform :  Arduino Due or (Arduino Mega2560 or Mega2560 PRO MINI)
  *  
  *  Version :          3.00
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
  *  2.0  Adaptation du code pour le passer en 21 bands :
        - Changement de lib si5351mcu to Adafruit_SI5351 car celle de pavel ne permet que 2 sorties activées en simultanné
        - Ajout des commentaires
        - Passage en lecture de 3 MSGEQ7
  *  3.0 Ajout des variateurs de lumières et de speed peak delais et annimation de setup   
  *
  *  ------
  *  Latest Version
  *  3.0
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
#include <Adafruit_SI5351.h>
Adafruit_SI5351 clockgen = Adafruit_SI5351();

#define PULSE_PIN        13 //inutilisé ?
#define NOISE_MSGEQ7     120

#define ROWS             20 //num of row MAX=20
#define COLUMNS          21 //num of column

#define DATA_PIN         9  //led data pin
#define STROBE_PIN       6  //MSGEQ7 strobe pin
#define RESET_PIN        7  //MSGEQ7 reset pin

#define MSGEQ7_1         A0
#define MSGEQ7_2         A1
#define MSGEQ7_3         A2
#define POT_LUM          A3
#define SET_PEAKPAUSE    A4
#define BTN_PIN          A6


#define NUMPIXELS ROWS* COLUMNS


struct Point {
  char x, y;
  char r, g, b;
  bool active;
};

struct TopPoint {
  int position;
  int peakpause;
};

Point spectrum[ROWS][COLUMNS];
TopPoint peakhold[COLUMNS];
int spectrumValue[COLUMNS];

int long counter =       0;
int long time_change =   0;


int RawLumVal =          0;
int Luminosite =         20;
int RawPeakPause =       0;
int SetPeakPause =       1;
int EffetSelect =        0;


Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMPIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(9600);

  if (clockgen.begin() != ERROR_NONE) {
    Serial.print("Ooops, no Si5351 detected ... Check your wiring or I2C ADDR!");
    while (1);
  }
  Serial.println("Init clk OK !");

  clockgen.setupPLL(SI5351_PLL_A, 24, 2, 3);

  clockgen.setupMultisynth(0, SI5351_PLL_A, 92, 0, 1);  //clk0 = 100kHz
  clockgen.setupRdiv(0, SI5351_R_DIV_64);

  clockgen.setupMultisynth(1, SI5351_PLL_A, 64, 0, 1);  //clk1 = 150kHz
  clockgen.setupRdiv(1, SI5351_R_DIV_64);

  clockgen.setupMultisynth(2, SI5351_PLL_A, 48, 0, 1);  //clk2 = 200kHz
  clockgen.setupRdiv(2, SI5351_R_DIV_64);

  clockgen.enableOutputs(true);

  pinMode(STROBE_PIN,   OUTPUT);
  pinMode(RESET_PIN,    OUTPUT);
  pinMode(DATA_PIN,     OUTPUT);
  pinMode(PULSE_PIN,    OUTPUT);  //inutilisé ?
  pinMode(POT_LUM,      INPUT);
  pinMode(BTN_PIN,      INPUT_PULLUP);

  pixels.setBrightness(20);       //set Brightness

  pixels.begin();
  pixels.show();

  digitalWrite(RESET_PIN,    LOW);   // reset bas
  digitalWrite(STROBE_PIN,   LOW);  // strobe bas
  delay(1);
  digitalWrite(RESET_PIN,    HIGH);  // reset haut
  delay(1);
  digitalWrite(RESET_PIN,    LOW);    // reset bad
  digitalWrite(STROBE_PIN,   HIGH);  // strobe haut
  delay(1);
 
 /* * * * * * * * * * * * * * * * * * * * * Séquence de démmarage * * * * * * * * * * * * * * * * * * * */

  /*
// Remplissage des lignes en "tombant" du haut vers le bas
  for (int i = ROWS - 1; i >= 0; i--) {
    for (int j = 0; j < COLUMNS; j++) {
      // Faire "tomber" les LEDs jusqu'à la ligne actuelle i
      for (int k = ROWS - 1; k >= i; k--) {
        pixels.setPixelColor(j * ROWS + k, pixels.Color(0, 0, 255)); // LED verte
      }
    }
    pixels.show(); // Mettre à jour l'affichage pour montrer les LEDs "tombées"
    delay(25); // Pause pour l'effet de "chute"
  }

  delay(50); // Pause lorsque tout est allumé

  // Vidage des lignes en "remontant" du bas vers le haut
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLUMNS; j++) {
      pixels.setPixelColor(j * ROWS + i, pixels.Color(0, 0, 0)); // Éteindre la LED
    }
    pixels.show(); // Mettre à jour l'affichage pour montrer les LEDs "remontées"
    delay(25); // Pause pour l'effet de "remontée"
  }*/

//  allumage dans le bon sens mais pas complet
/*
  
  for (int row = ROWS - 1; row >= 0; row--) {
    // Allumer la rangée en bleu
    for (int col = 0; col < COLUMNS; col++) {
      spectrum[row][col].active = true;
      spectrum[row][col].r = 0;    
      spectrum[row][col].g = 0;    
      spectrum[row][col].b = 255;  // Couleur bleue
    }
    flushMatrix();  // Affiche l'état actuel des LEDs
    delay(50);      // Court délai avant d'éteindre la ligne

    // Éteindre la rangée immédiatement après
    for (int col = 0; col < COLUMNS; col++) {
      spectrum[row][col].active = true;
      spectrum[row][col].r = 0;  
      spectrum[row][col].g = 0; 
      spectrum[row][col].b = 0;  // Éteindre la LED
    }
    flushMatrix();  // Affiche l'état actuel des LEDs
    delay(2);       // Très court délai avant de passer à la rangée suivante
  }
  */
  //  complet mais allumage dans le mauvais sens

  for (int bottomRow = ROWS - 1; bottomRow >= 0; bottomRow--) {
    // Faire "tomber" une ligne bleue du haut jusqu'à la position bottomRow
    for (int currentRow = 0; currentRow <= bottomRow; currentRow++) {
      // Allumer la ligne actuelle en bleu
      for (int col = 0; col < COLUMNS; col++) {
        spectrum[currentRow][col].active = true;
        spectrum[currentRow][col].r = 0;
        spectrum[currentRow][col].g = 0;
        spectrum[currentRow][col].b = 255;
      }
      flushMatrix();
      delay(50); // Temps pour la chute de la ligne
      
      // Si ce n'est pas la ligne finale, éteindre la ligne actuelle avant de "tomber" plus bas
      if (currentRow < bottomRow) {
        for (int col = 0; col < COLUMNS; col++) {
          spectrum[currentRow][col].active = false;
          spectrum[currentRow][col].r = 0;
          spectrum[currentRow][col].g = 0;
          spectrum[currentRow][col].b = 0;
        }
        flushMatrix();
      }
    }
    // Pas besoin d'éteindre la ligne bottomRow, car elle doit rester allumée
    delay(50); // Délai après avoir rempli la ligne
  }

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * Main loop * * * * * * * * * * * * * * * * * * * * * * * * */
void loop() {
  counter++;


  //if (counter == 50) {

  RawLumVal = analogRead(POT_LUM);
  RawPeakPause = analogRead(SET_PEAKPAUSE);

  //Serial.print("RAWLuminosite: ");
  //Serial.print(RawLumVal);

  Luminosite = map(RawLumVal, 0, 1023, 20, 200);
  //Serial.print("        Luminosite: ");
  //Serial.print(Luminosite);

  //Serial.print("     |      ");

  //Serial.print("RAWPeakPause: ");
  //Serial.print(RawPeakPause);

  SetPeakPause = map(RawPeakPause, 0, 1023, 0, 10);
  //Serial.print("        PeakPause: ");
  //Serial.print(SetPeakPause);


  if (digitalRead(BTN_PIN) == LOW) {
    while (digitalRead(BTN_PIN) == LOW)
      ;  // Attendre que le bouton soit relâché

    if (EffetSelect >= 10) EffetSelect = 0;
    else EffetSelect++;
  }
  //Serial.print("    |     ");
  //Serial.print("Effet: ");
  //Serial.print(EffetSelect);
  //Serial.println("");


  //counter = 0;
  //}

  //Serial.print("Counter :");

  //Serial.println(counter);


  clearspectrum();
  pixels.setBrightness(Luminosite);  //set Brightness

  digitalWrite(RESET_PIN, HIGH);
  delayMicroseconds(3000);
  digitalWrite(RESET_PIN, LOW);

  //*** lecture valeurs analogiques des MSGEQ7 ***//
  for (int i = 0; i < COLUMNS; i++) {

    digitalWrite(STROBE_PIN, LOW);

    delayMicroseconds(1000);  //1ms


    spectrumValue[i] = analogRead(MSGEQ7_1);                    //lecture 1er MSGEQ7
    if (spectrumValue[i] < NOISE_MSGEQ7) spectrumValue[i] = 0;  //niv minimum ?
    spectrumValue[i] = constrain(spectrumValue[i], 0, 1023);
    spectrumValue[i] = map(spectrumValue[i], 0, 1023, 0, ROWS);
    i++;  //passage au valeurs du MSGEQ7 2


    spectrumValue[i] = analogRead(MSGEQ7_2);  //lecture 2eme MSGEQ7
    if (spectrumValue[i] < NOISE_MSGEQ7) spectrumValue[i] = 0;
    spectrumValue[i] = constrain(spectrumValue[i], 0, 1023);
    spectrumValue[i] = map(spectrumValue[i], 0, 1023, 0, ROWS);
    i++;  //passage au valeurs du MSGEQ7 3


    spectrumValue[i] = analogRead(MSGEQ7_3);  //lecture 3eme MSGEQ7
    if (spectrumValue[i] < NOISE_MSGEQ7) spectrumValue[i] = 0;
    spectrumValue[i] = constrain(spectrumValue[i], 0, 1023);
    spectrumValue[i] = map(spectrumValue[i], 0, 1023, 0, ROWS);



    digitalWrite(STROBE_PIN, HIGH);  //changement de freq/sortie du MSGEQ7 (freq suivante)
  }
  /*************************************************/

  //allumage des LED en commencant par le bas
  for (int j = 0; j < COLUMNS; j++) {
    for (int i = 0; i < spectrumValue[j]; i++) {
      spectrum[i][COLUMNS - 1 - j].active = 1;  // active la LED dans la position calculée
      //ajustent la couleur de la LED (dans ce cas, vert)
      spectrum[i][COLUMNS - 1 - j].r = 0;    //COLUMN Color red
      spectrum[i][COLUMNS - 1 - j].g = 100;  //COLUMN Color green
      spectrum[i][COLUMNS - 1 - j].b = 150;  //COLUMN Color blue
    }

    //allumage des peaks
    if (spectrumValue[j] - 1 > peakhold[j].position) {
      // Si le spectre actuel est plus élevé que le pic précédemment enregistré, le nouveau pic est mis à jour.

      // Les lignes suivantes éteignent la LED du pic précédent (en mettant ses composants couleur à 0).
      spectrum[spectrumValue[j] - 1][COLUMNS - 1 - j].r = 0;
      spectrum[spectrumValue[j] - 1][COLUMNS - 1 - j].g = 0;
      spectrum[spectrumValue[j] - 1][COLUMNS - 1 - j].b = 0;

      // Mettre à jour la position du nouveau pic.
      peakhold[j].position = spectrumValue[j] - 1;

      // Réinitialiser le délai pendant lequel le pic est maintenu (peakpause) avant de commencer à "retomber".
      peakhold[j].peakpause = SetPeakPause;  //set peakpause
    }

    else {
      // Si le spectre actuel n'est pas plus élevé que le pic enregistré,
      // le pic précédent reste allumé et ne change pas de position.

      // Activer le pic (LED) à sa position précédente.
      spectrum[peakhold[j].position][COLUMNS - 1 - j].active = 1;
      // couleur active : jaune (rouge + vert)
      spectrum[peakhold[j].position][COLUMNS - 1 - j].r = 255;  //Peak Color red
      spectrum[peakhold[j].position][COLUMNS - 1 - j].g = 0;    //Peak Color green
      spectrum[peakhold[j].position][COLUMNS - 1 - j].b = 0;    //Peak Color blue
    }
  }

  flushMatrix();

  if (counter % 3 == 0)
    topSinking();  //peak delay
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * Fin loop * * * * * * * * * * * * * * * * * * * * * * * * */



//************************************************//
// * * * * * * * * Fct TopSinking * * * * * * * * //
//************************************************//
void topSinking() {
  for (int j = 0; j < ROWS + 1; j++)  // Boucle à travers touts les étages de la matrice d'affichage
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

//************************************************//
// * * * * * * * Fct ClearSpectrum * * * * * * * *//
//************************************************//
void clearspectrum() {
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLUMNS; j++) {
      spectrum[i][j].active = false;
    }
  }
}

//************************************************//
// * * * * * * * * Fct FlushMatrix * * * * * * * *//
//************************************************//
void flushMatrix() {
  for (int j = 0; j < COLUMNS; j++) {
    if (j % 2 != 0)  // Vérifie si l'index de la colonne est impair. Cela est fait pour créer un effet zébré ou pour compenser pour un certain arrangement physique des LEDs.
    {
      for (int i = 0; i < ROWS; i++)  // Si la colonne est impaire, itère à travers toutes les lignes de la matrice.
      {
        if (spectrum[ROWS - 1 - i][j].active)  // Vérifie si le LED à la position actuelle est actif.
        {
          // Si le LED est actif, définit sa couleur. Notez que les coordonnées sont inversées pour les colonnes impaires.
          // La méthode setPixelColor prend l'index du LED et les valeurs RGB pour la couleur.
          pixels.setPixelColor(j * ROWS + i, pixels.Color(
                                               spectrum[ROWS - 1 - i][j].r,
                                               spectrum[ROWS - 1 - i][j].g,
                                               spectrum[ROWS - 1 - i][j].b));
        } else {
          pixels.setPixelColor(j * ROWS + i, 0, 0, 0);  // Si le LED n'est pas actif, définit sa couleur à noir, ce qui l'éteint en pratique.
        }
      }
    } else  // Si la colonne est paire.
    {
      for (int i = 0; i < ROWS; i++)  // Vérifie si le LED à la position actuelle est actif.
      {
        if (spectrum[i][j].active) {
          // Si le LED est actif, définit sa couleur. La coordination est standard pour les colonnes paires.
          pixels.setPixelColor(j * ROWS + i, pixels.Color(
                                               spectrum[i][j].r,
                                               spectrum[i][j].g,
                                               spectrum[i][j].b));
        } else {
          pixels.setPixelColor(j * ROWS + i, 0, 0, 0);  // Si le LED n'est pas actif, définit sa couleur à noir, ce qui l'éteint en pratique.
        }
      }
    }
  }

  pixels.show();  // Après avoir parcouru et mis à jour toutes les LEDs, la méthode 'show' est appelée pour appliquer les changements sur la matrice de LEDs.
}
