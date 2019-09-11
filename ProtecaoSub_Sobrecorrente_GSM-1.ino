//  Proteção de sobrecorrente (motor travado, rolamento com atrito, etc) e sub corrente (falta dágua na bomba).
//  Criado em 02/2018 por Dimy Ricardo Disconzi (dimydisconzi@gmail.com / www.dimydisconzi.com.br )
//  -> Ampliar para 3 fases
//  Através da medição da tensão sobre o resistor shunt ligado ao TC (Tansformador de Corrente), ceonverte-se este valor discreto de 12 bits
//  no valor da corrente através da equação definida pelos coeficientes A (Angular) e B (Linear).
//  O algoritmo inicia aguardando a coluna de água chegar ao alto (Parâmetro

#define NomeDoLevante "Levante do Capão Das Varas"  //Mudar para cada diferente levante.
#define Botao PB15                  // PB15 -> chave para SETUP automático das correntes
#define LED PC13                    // PC13 -> LED onboard, piscará e vezes indicando que está sendo feito o setup automático ao precionar o Botao.
#define SensorDeCorrente1 PA0       // Pino do sensor de corrente A0, monitora a corrente da fase A.
#define HabilitaMotor PB12          // PB12 -> Pino que mantém acionado o relê (NA, ficando Fechado "estado On") que mantém a corrente da bobina do acionador ativada, e
// PB12 é desligado quando a corrente medida ciclicamente estive fora dos limites estabelecidos por KSubI e KSobreI.
#define TempoParaEntrarNoSetup 3    // Tempo de botão precionado que faz a corrente atual do sensor ser adotada como referência (E já recalcula os limites com base nas constantes).
#define KSubI 0.7                   // Constante de SubCorrente = 0.7 "70% do valor de regime"(confirmar constantes)
#define KSobreI 1.08                // Constante de SobreCorrente = 1.08 " 108% a corrente de regime"  (confirmar constantes)   
#define TempoDeTolerancia 10   //!!! usar 10 pelo menos  // Tempo em segundos durante o qual a proteção tolera a alteração na corrente
#define TempoColunaDagua 5     //!!!! verificar tempo real, usar no mínimo uns 20 ou 30 s. Espera estabilizar corrente
#define NumeroDeAmostrasParaReferencia 10  //Número de amostras que serão adquiridas para determinar a corrente de Referência (Media)
#define A 4/566       //Coeficiente angular da equação de converção do valor discreto 12 bits da AN0 para valor real de corrente
#define B 63835/566   //Coeficiente linear...

#define SIM800_TX_PIN 10  //TX do SIM800L conectado ao pino 10
#define SIM800_RX_PIN 11  //RX do SIM800L conectado ao pino 11


#include <Sim800l.h>        // GSM: Biblioteca do shield GSM
#include <SoftwareSerial.h> // GSM: 
Sim800l Sim800l;
/*char* textoFalhaNaBomba = "Falha na bomba do levante do Capão das Varas";
  char* textoSobreCorrente = "Sobrecorrente!";
  char* textoSubCorrente = "Subcorrente";*/           Talvez deixar as mensagens como váriáveis locais nos procedures de mensagemGSM

// Váriáveis
boolean buttonState = LOW;        //  Armazena momentaneamente valor de Botao.
int SubCorrente = 0;              //  Indica o valor mínimo aceitável para a corrente. (SubCorrente = KSubI * CorrenteReferencia)
int SobreCorrente = 0;            //  Armazenará  o valor máximo aceitável para a corrente. (SobreCorrente = KSobreI * CorrenteReferencia)
int CorrenteReferencia = 0;       //  Armazenará o valor de REFÊNCIA da corrente do motor (Corrente de regime permanente, aprox. a corrente Nominal )
int CorrenteAtual = 0;            //  Armazena leitura do sensor de corrente ciclicamente para verificação em tempo real. (Compara com os limites definidos)
char* CelularDimy51 = "01551996096581";  //  NÚMERO DO CELULAR QUE RECEBE O AVISO DE EMERGÊNCIA!

/*void AguardarPartida(); void DebounceSetupDeCorrente(); void SetupCorrenteReferencia(); void VerificarMotor(); void InformarValoresSetados(); int ConverterV2I(int);
  int ConverterV2I(); void EnviarSMS();*/

void setup() {
  pinMode(Botao, INPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);            //LED fica apagado com nível HIGH
  pinMode(HabilitaMotor, OUTPUT);
  digitalWrite(HabilitaMotor, HIGH);  //Habilita ativar motor sem comparar corrente (Permite o motor partir com alta corrente transitória)
  Serial.begin(2000000);
  delay(1000);
  Serial.write("Inicializando... Realizando ajustes de corrente e parâmetros.");
  Sim800l.begin(); // Inicializa a biblioteca GSM.
  AguardarPartida();                  // Aguarda que haja corrente no sensor (motor ligado)
  delay(1000 * TempoColunaDagua); //Aguarda coluna de água chegar ao alto do levante (estabilizar corrente)
  SetupCorrenteReferencia();
}

int ConverterV2I(int CorrenteDiscreta12bits) {
  int CorrenteConvertida;
  CorrenteConvertida = CorrenteDiscreta12bits * A + B;
  return CorrenteConvertida;
}

void AguardarPartida() {
  CorrenteAtual = analogRead(SensorDeCorrente1);
  CorrenteAtual = ConverterV2I(CorrenteAtual);
  while (CorrenteAtual < 5) { //para o caso de haver indução/ruído no sensor (no caso ideal seria igual a zero)
    Serial.println("Corrente medida próxima de 0 Amperes. Aguardando Partida do Motor. Nova tentativa em 2 segundos.");
    delay(2000);
  }
}

void InformarValoresSetados() {
  Serial.println ("-> INFORMANDO VALORES DE REFÊRENCIA SETADOS <-");
  Serial.print("  Corrente de regime permanente (Referencia): "); Serial.print(CorrenteReferencia); Serial.println(" A");
  Serial.print("  Limite superior de Sobrecorrente: "); Serial.print(SobreCorrente); Serial.println(" A");
  Serial.print("  Limite inferior de Subcorrente: "); Serial.print(SubCorrente); Serial.println(" A");
}

void DebounceSetupDeCorrente() {
  delay(3000);  // Debounce e tempo extra para motor estabilizar
  buttonState = digitalRead(Botao);
  if (buttonState == HIGH) {         //se continua precionado botão...Debounced
    Serial.print("Setup de corrente: ");
  }
  SetupCorrenteReferencia();
}

void SetupCorrenteReferencia() {
  int Amostra = 0;
  CorrenteReferencia = 0;   //Reseta Variável
  while (Amostra < NumeroDeAmostrasParaReferencia) {
    CorrenteReferencia = CorrenteReferencia + (analogRead(SensorDeCorrente1)); // Corrente de regime permanente do equipamento.
    digitalWrite(LED, LOW);
    delay(250);
    digitalWrite(LED, HIGH);
    delay(250);
    Amostra++;
  }
  CorrenteReferencia = CorrenteReferencia / NumeroDeAmostrasParaReferencia;
  CorrenteReferencia = ConverterV2I(CorrenteReferencia);
  SubCorrente = KSubI * CorrenteReferencia;     //Limite infereior para correntes
  SobreCorrente = KSobreI * CorrenteReferencia; //Limite Superior para correntes
  Serial.println(""); Serial.println(">>>>>>>>  SETUP REALIZADO COM SUCESSO  <<<<<<<<<<<"); Serial.println("");
  InformarValoresSetados();
}

void  VerificarMotor() {
  CorrenteAtual = analogRead(SensorDeCorrente1);
  CorrenteAtual = ConverterV2I(CorrenteAtual);
  Serial.println(""); Serial.print("Corrente Instantânea (Atual): "); Serial.print(CorrenteAtual); Serial.println(" A");
  if (CorrenteAtual > SobreCorrente) {      //Corrente de regime está fora dos limites?
    delay(1000 * TempoDeTolerancia);        //Aguarda 10s pois pode ser a partida do motor ou um transitório momentâneo
    CorrenteAtual = analogRead(SensorDeCorrente1);
    CorrenteAtual = ConverterV2I(CorrenteAtual);
    if (CorrenteAtual > SobreCorrente) {    //se a CORRENTE CONTINUA ACIMA?
      digitalWrite(HabilitaMotor, LOW);     //Desliga motor
      Serial.print(">>>>>>>>>>>>ATENÇÃO!<<<<<<<<<<<<<<<<");
      Serial.print("PROBLEMA No "); Serial.println(NomeDoLevante);
      Serial.println(">>>> Desligando equipamento<<<<<<: SOBRECORRENTE DETECTADA POR MAIS QUE O PERÍODO DE TOLERÂNCIA.");
      Serial.print("SOBRECORRENTE MEDIDA: "); Serial.print(CorrenteAtual); Serial.println(" Amperes");
      Serial.println("CHAMAR Adelmiro ou Demétrio.");
      //    EnviarSMS("SobreCorrente");
    }
  }
  if (CorrenteAtual < SubCorrente) {        //Corrente de regime está fora dos limites? Possível que a ÁGUA EM NÍVEL BAIXO!
    delay(1000 * TempoDeTolerancia);        //Aguarda 10s pois pode ser um transitório momentâneo...
    if (CorrenteAtual < SubCorrente) {      //Corente continua baixa?
      digitalWrite(HabilitaMotor, LOW);     //Desliga motor
      Serial.print("PROBLEMA No "); Serial.println(NomeDoLevante);
      Serial.println("Desligando equipamento: SUBCORRENTE DETECTADA POR MAIS QUE O PERÍODO DE TOLERÂNCIA.");
      Serial.println("Nível de água na vala pode estar abaixo do mínimo. Chamar Adelmiro ou Raimundo");
      Serial.print("SUBCORRENTE MEDIDA: "); Serial.print(CorrenteAtual); Serial.println(" Amperes");
      Serial.println("CHAMAR Adelmiro ou Demétrio.");
      //    EnviarSMS("Mensagem automáticaSubcorrente");
    }
  }
}

void EnviarSMS(char* msg) {
  bool error;
  error = Sim800l.sendSms(CelularDimy51, msg);
  if (error) Serial.println("Erro no envio da mensagem!");
  Serial.println("Mensagem enviada!");
}

void loop() {
  buttonState = digitalRead(Botao);
  while (buttonState == HIGH) {
    DebounceSetupDeCorrente();
  }
  VerificarMotor();
  delay(2000);
  InformarValoresSetados();
}

