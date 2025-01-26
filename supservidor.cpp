#include <iostream>     /* cerr */
#include <algorithm>
#include "supservidor.h"

using namespace std;

/* ========================================
   CLASSE SUPSERVIDOR
   ======================================== */

/// Construtor
SupServidor::SupServidor()
  : Tanks()
  , server_on(false)
  , LU()
  /*ACRESCENTAR*/
  , thr_server()
  , sock_server()
{
  // Inicializa a biblioteca de sockets
  /*ACRESCENTAR*/
  mysocket_status iResult = mysocket::init();
  // Em caso de erro, mensagem e encerra
  if (iResult != mysocket_status::SOCK_OK)
  {
    cerr <<  "Biblioteca mysocket nao pode ser inicializada";
    exit(-1);
  }
}

/// Destrutor
SupServidor::~SupServidor()
{
  // Deve parar a thread do servidor
  server_on = false;

  // Fecha todos os sockets dos clientes
  for (auto& U : LU) U.close();
  // Fecha o socket de conexoes
  sock_server.close();

  // Espera o fim da thread do servidor
  if (thr_server.joinable()) thr_server.join();
  thr_server = thread();

  // Encerra a biblioteca de sockets
  mysocket::end();
}

/// Liga o servidor
bool SupServidor::setServerOn()
{
  // Se jah estah ligado, nao faz nada
  if (server_on) return true;

  // Liga os tanques
  setTanksOn();

  // Indica que o servidor estah ligado a partir de agora
  server_on = true;

  try
  {
    // Coloca o socket de conexoes em escuta
    /*ACRESCENTAR*/
    mysocket_status iResult = sock_server.listen(SUP_PORT);
    // Em caso de erro, gera excecao
    if (iResult != mysocket_status::SOCK_OK) throw 1;

    // Lanca a thread do servidor que comunica com os clientes
    /*ACRESCENTAR*/
    thr_server = thread([this] { thr_server_main(); });
    // Em caso de erro, gera excecao
    if (!thr_server.joinable()) throw 2;
  }
  catch(int i)
  {
    cerr << "Erro " << i << " ao iniciar o servidor\n";

    // Deve parar a thread do servidor
    server_on = false;

    // Fecha o socket do servidor
    sock_server.close();

    return false;
  }

  // Tudo OK
  return true;
}

/// Desliga o servidor
void SupServidor::setServerOff()
{
  // Se jah estah desligado, nao faz nada
  if (!server_on) return;

  // Deve parar a thread do servidor
  server_on = false;

  // Fecha todos os sockets dos clientes
  for (auto& U : LU) U.close();
  // Fecha o socket de conexoes
  /*ACRESCENTAR*/
  sock_server.close();

  // Espera pelo fim da thread do servidor
  /*ACRESCENTAR*/
  if (thr_server.joinable()) thr_server.join();
  thr_server = thread();

  // Faz o identificador da thread apontar para thread vazia
  /*ACRESCENTAR*/
  thr_server = thread();

  // Desliga os tanques
  setTanksOff();
}

/// Leitura do estado dos tanques
void SupServidor::readStateFromSensors(SupState& S) const
{
  // Estados das valvulas: OPEN, CLOSED
  S.V1 = v1isOpen();
  S.V2 = v2isOpen();
  // Niveis dos tanques: 0 a 65535
  S.H1 = hTank1();
  S.H2 = hTank2();
  // Entrada da bomba: 0 a 65535
  S.PumpInput = pumpInput();
  // Vazao da bomba: 0 a 65535
  S.PumpFlow = pumpFlow();
  // Estah transbordando (true) ou nao (false)
  S.ovfl = isOverflowing();
}

/// Leitura e impressao em console do estado da planta
void SupServidor::readPrintState() const
{
  if (tanksOn())
  {
    SupState S;
    readStateFromSensors(S);
    S.print();
  }
  else
  {
    cout << "Tanques estao desligados!\n";
  }
}

/// Impressao em console dos usuarios do servidor
void SupServidor::printUsers() const
{
  for (const auto& U : LU)
  {
    cout << U.login << '\t'
         << "Admin=" << (U.isAdmin ? "SIM" : "NAO") << '\t'
         << "Conect=" << (U.isConnected() ? "SIM" : "NAO") << '\n';
  }
}

/// Adicionar um novo usuario
bool SupServidor::addUser(const string& Login, const string& Senha,
                             bool Admin)
{
  // Testa os dados do novo usuario
  if (Login.size()<6 || Login.size()>12) return false;
  if (Senha.size()<6 || Senha.size()>12) return false;

  // Testa se jah existe usuario com mesmo login
  auto itr = find(LU.begin(), LU.end(), Login);
  if (itr != LU.end()) return false;

  // Insere
  LU.push_back( User(Login,Senha,Admin) );

  // Insercao OK
  return true;
}

/// Remover um usuario
bool SupServidor::removeUser(const string& Login)
{
  // Testa se existe usuario com esse login
  auto itr = find(LU.begin(), LU.end(), Login);
  if (itr == LU.end()) return false;

  // Remove
  LU.erase(itr);

  // Remocao OK
  return true;
}

/// A thread que implementa o servidor.
/// Comunicacao com os clientes atraves dos sockets.
void SupServidor::thr_server_main(void)
{
  LUitr iU;

  SupState S;

  // Fila de sockets para aguardar chegada de dados
  /*ACRESCENTAR*/
  mysocket_queue Q;

  //Variável result para armazenar o resultado da espera
  mysocket_status iResult;

  while (server_on)
  {
    // Erros mais graves que encerram o servidor
    // Parametro do throw e do catch eh uma const char* = "texto"
    try
    {
      // Encerra se o socket de conexoes estiver fechado
      if (!sock_server.accepting())
      {
        throw "socket de conexoes fechado";
      }

      // Inclui na fila de sockets todos os sockets que eu
      // quero monitorar para ver se houve chegada de dados

      // Limpa a fila de sockets
      /*ACRESCENTAR*/
      Q.clear();

      // Inclui na fila o socket de conexoes
      /*ACRESCENTAR*/
      Q.include(sock_server);

      // Inclui na fila todos os sockets dos clientes conectados
      /*ACRESCENTAR*/
      for (auto& U : LU)
      {
        if (U.isConnected()) Q.include(U.sock);
      }
      // Espera ateh que chegue dado em algum socket (com timeout)
      /*ACRESCENTAR*/
      iResult = Q.wait_read(SUP_TIMEOUT*1000);

      // De acordo com o resultado da espera:
      // SOCK_TIMEOUT:
      // Saiu por timeout: nao houve atividade em nenhum socket
      // Aproveita para salvar dados ou entao nao faz nada
      // SOCK_ERROR:
      // Erro no select: encerra o servidor
      // SOCK_OK:
      // Houve atividade em algum socket da fila:
      //   Testa se houve atividade nos sockets dos clientes. Se sim:
      //   - Leh o comando
      //   - Executa a acao
      //   = Envia resposta
      //   Depois, testa se houve atividade no socket de conexao. Se sim:
      //   - Estabelece nova conexao em socket temporario
      //   - Leh comando, login e senha
      //   - Testa usuario
      //   - Se deu tudo certo, faz o socket temporario ser o novo socket
      //     do cliente e envia confirmacao
      switch (iResult) {
        case mysocket_status::SOCK_TIMEOUT:
          // Nao faz nada
          break;
        case mysocket_status::SOCK_ERROR:
          throw 2;
          break;
        case mysocket_status::SOCK_OK:
          try {
            //Testa se houve atividade nos sockets dos clientes
            for (iU = LU.begin(); iU != LU.end(); ++iU){
              if (server_on && iU->isConnected() && Q.had_activity(iU->sock)) {
                //Leh o comando
                int16_t cmd;
                iResult = iU->sock.read_int16(cmd);
                //Executa a acao
                if (iResult == mysocket_status::SOCK_OK) {
                  string login, senha;
                  uint16_t Open, Input;
                  SupState S;
                  switch (cmd) {
                    //Testa se houve atividade nos sockets dos clientes. Se sim:
                      //   - Leh o comando
                      //   - Executa a acao
                      //   = Envia resposta
                      case CMD_LOGIN:
                        //Leh login e senha
                        iResult = iU->sock.read_string(login);
                        iResult = iU->sock.read_string(senha);
                        //Testa usuario
                        if (iU->login == login && iU->password == senha) {
                          //Se deu tudo certo, faz o socket temporario ser o novo socket
                          //do cliente e envia confirmacao
                          iU->sock.write_int16(iU->isAdmin ? CMD_ADMIN_OK : CMD_OK);
                        } else {
                          //Se deu tudo errado, envia erro
                          iU->sock.write_int16(CMD_ERROR);
                        }
                        break;
                      case CMD_GET_DATA:
                        //Leh o estado dos tanques
//Testa se houve atividade no socket de conexao
            if (server_on && Q.had_activity(sock_server) && sock_server.connected()) {
              //Estabelece nova conexao em socket temporario
              tcp_mysocket sock_temp;
              iResult = sock_server.accept(sock_temp);
              //Leh comando, login e senha
              int16_t cmd;
              iResult = sock_temp.read_int16(cmd);
              string login, senha;
              iResult = sock_temp.read_string(login);
              iResult = sock_temp.read_string(senha);
              //Testa usuario
              auto itr = find(LU.begin(), LU.end(), login);
              if (itr != LU.end() && itr->password == senha) {
                //Se deu tudo certo, faz o socket temporario ser o novo socket
                //do cliente e envia confirmacao
                itr->sock.swap(sock_temp);
                itr->sock.write_int16(itr->isAdmin ? CMD_ADMIN_OK : CMD_OK);
              } else {
                //Se deu tudo errado, envia erro
                sock_temp.write_int16(CMD_ERROR);
                sock_temp.close();
              }
            }
                        readStateFromSensors(S);
                        //Envia o estado dos tanques
                        iU->sock.write_int16(CMD_GET_DATA);
                        iU->sock.write_uint16(S.H1);
                        iU->sock.write_uint16(S.H2);
                        iU->sock.write_uint16(S.PumpInput);
                        iU->sock.write_uint16(S.PumpFlow);
                        iU->sock.write_uint16(S.V1);
                        iU->sock.write_uint16(S.V2);
                        iU->sock.write_uint16(S.ovfl);
                        break;
                      case CMD_SET_V1:
                        //Envia mensagem avisando que o comando foi recebido
                        iU->sock.write_int16(CMD_SET_V1);
                        //Leh o parametro do comando
                        uint16_t Open;
                        iResult = iU->sock.read_uint16(Open);
                        //Executa a acao
                        if (iResult == mysocket_status::SOCK_OK && iU->isAdmin) {
                          setV1Open(Open != 0);
                          //Envia resposta
                          iU->sock.write_int16(CMD_OK);
                        } else {
                          //Envia erro
                          iU->sock.write_int16(CMD_ERROR);
                        }
                        break;
                      case CMD_SET_V2:
                        //Envia mensagem avisando que o comando foi recebido
                        iU->sock.write_int16(CMD_SET_V2);
                        //Leh o parametro do comando
                        iResult = iU->sock.read_uint16(Open);
                        //Executa a acao
                        if (iResult == mysocket_status::SOCK_OK && iU->isAdmin) {
                          setV2Open(Open != 0);
                          //Envia resposta
                          iU->sock.write_int16(CMD_OK);
                        } else {
                          //Envia erro
                          iU->sock.write_int16(CMD_ERROR);
                        }
                        break;
                      case CMD_SET_PUMP:
                        //Envia mensagem avisando que o comando foi recebido
                        iU->sock.write_int16(CMD_SET_PUMP);
                        //Leh o parametro do comando
                        uint16_t Input;
                        iResult = iU->sock.read_uint16(Input);
                        //Executa a acao
                        if (iResult == mysocket_status::SOCK_OK && iU->isAdmin) {
                          setPumpInput(Input);
                          //Envia resposta
                          iU->sock.write_int16(CMD_OK);
                        } else {
                          //Envia erro
                          iU->sock.write_int16(CMD_ERROR);
                        }
                        break;
                      default:
                        //Envia erro
                        iU->sock.write_int16(CMD_ERROR);
                        break;

                  }
                }
              }
            }
          }
        //Catch se houve erro na comunicação
        catch (const char* err) {
          cerr << "Erro na comunicacao com o cliente: " << err << endl;
          iU->sock.close();
        }
        //Testa se houve atividade no socket de conexao
        if (server_on && Q.had_activity(sock_server)) {
          //Estabelece nova conexao em socket temporario
          tcp_mysocket sock_temp;
          iResult = sock_server.accept(sock_temp);
          //Leh comando, login e senha
          int16_t cmd;
          iResult = sock_temp.read_int16(cmd);
          string login, senha;
          iResult = sock_temp.read_string(login);
          iResult = sock_temp.read_string(senha);
          //Testa usuario
          iU = find(LU.begin(), LU.end(), login);
          if (iU != LU.end() && iU->password == senha) {
            //Se deu tudo certo, faz o socket temporario ser o novo socket
            //do cliente e envia confirmacao
            iU->sock.swap(sock_temp);
            iU->sock.write_int16(iU->isAdmin ? CMD_ADMIN_OK : CMD_OK);
          } else {
            //Se deu tudo errado, envia erro
            sock_temp.write_int16(CMD_ERROR);
            sock_temp.close();
          }
        }
      }
    }
        // fim try - Erros mais graves que encerram o servidor
    catch(const char* err)  // Erros mais graves que encerram o servidor
    {
      cerr << "Erro no servidor: " << err << endl;

      // Sai do while e encerra a thread
      server_on = false;

      // Fecha todos os sockets dos clientes
      for (auto& U : LU) U.close();
      // Fecha o socket de conexoes
      /*ACRESCENTAR*/
      sock_server.close();

      // Os tanques continuam funcionando

    } // fim catch - Erros mais graves que encerram o servidor
  } // fim while (server_on)
}




