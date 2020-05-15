/**
 * @file      main.h
 * @brief     CityATM/ UDVeo - UDP to MQTT Gateway
 * 
 * This application receives UDP and forwards them without changes to a MQTT broker.
 *
 * @author    Jannik Beyerstedt <beyerstedt@consider-it.de>
 * @copyright (c) consider it GmbH, 2020
 */

#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <MQTTClient.h>

#include "version.h"

// static configuration values
#define UDP_BUFFER_SIZE 2048

// default values
#define CONF_FILE "/etc/udpmqttgw.conf"
#define MQTT_QOS 0
#define MQTT_KEEP_ALIVE 20      // seconds
#define MQTT_RETRY 1000         // milliseconds
#define MQTT_CONN_TIMEOUT 1000  // milliseconds
#define MQTT_VERSION MQTTVERSION_DEFAULT
#define MQTT_VERSION_STR "Default"
#define MQTT_SSL MQTT_SSL_VERSION_TLS_1_2
#define MQTT_SSL_STR "1.2"

/**
 * @brief Helper class to parse CLI arguments
 * 
 * CLI arguments can be specified with an equals sign between the parameter name and the
 * value, so for `-o=foobar.txt` the option would be `-o`.
 */
struct AppOptions {
  int         verbosity;
  std::string confPath{CONF_FILE};

  // config file options
  int         inputUdpPort{};
  std::string mqttUrl{};
  std::string mqttTopic{};
  std::string mqttClientID{};
  std::string mqttUsername{};                            // optional
  std::string mqttPassword{};                            // optional, if no username
  int         mqttVersion{MQTT_VERSION};                 // optional
  std::string mqttVersion_str{MQTT_VERSION_STR};         // just for debug output
  int         mqttQosLevel{MQTT_QOS};                    // optional
  int         mqttKeepAliveInterval{MQTT_KEEP_ALIVE};    // optional
  int         mqttRetryInterval{MQTT_RETRY};             // optional
  int         mqttConnectionTimeout{MQTT_CONN_TIMEOUT};  // optional

  int         mqttSslEnableServerCertAuth{1};    // optional, library default is 1
  int         mqttSslVersion{MQTT_SSL};          // optional
  std::string mqttSslVersion_str{MQTT_SSL_STR};  // just for debug output
  int         mqttSslVerify{0};                  // optional, library default is 0
  std::string mqttSslTrustStore{};               // optional
  std::string mqttSslKeyStore{};                 // optional
  std::string mqttSslPrivateKey{};               // optional
  std::string mqttSslPrivateKeyPasswd{};         // optional

  /**
   * @brief Constructor of the application options parser
   * 
   * The constructor parses the CLI arguments to set up the initial config values.
   * Additional parameters will be read from the specified config file by calling
   * parseConfFile().
   */
  AppOptions(int argc, char* argv[] /* NOLINT(modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays) */) :
      verbosity{0},
      applicationName{argv[0]} {
    std::vector<std::string> cliArgs{};
    for (int i = 1; i < argc; i++) {
      cliArgs.emplace_back(argv[i]);
    }

    while (!cliArgs.empty()) {
      auto arg = *cliArgs.begin();

      // find flags (but only at the beginning)
      if (0 == arg.find("-h")) {
        printUsage();
        exit(EXIT_SUCCESS);
        break;
      }
      if (0 == arg.find("-v")) {
        this->verbosity = 1;

      } else if (0 == arg.find("-c=")) {
        this->confPath = arg.substr(arg.find('=') + 1);

      } else {
        printUsageShort();
        std::cerr << "error: unrecognized arguments: " << arg << "\n";
        exit(EXIT_FAILURE);
        break;
      }

      // remove the parsed argument from the vector
      cliArgs.erase(cliArgs.begin());
    }
  }

  /**
   * @brief Parse the .conf file specified by the correponding CLI option
   * 
   * @return    Returns False, if the configuration is invalid
   * @exception Will throw a runtime_error, if file could not be opened or has invalid syntax.
   * @exception Will throw intalid_argument, if integer values could not be parsed
   */
  bool parseConfFile() {
    std::fstream confFile;
    confFile.open(this->confPath, std::ios::in);
    if (!confFile.is_open()) {
      std::cerr << "[ERROR] Unable to open config file\n";
      throw std::runtime_error("Unable to open config file");
    }

    // parse config file
    std::string confLine;
    int         lineNum{0};
    while (std::getline(confFile, confLine)) {
      lineNum++;
      confLine = trimComment(confLine);
      confLine = trim(confLine);

      if (confLine.empty()) {
        continue;
      }

      // interpret config parameters
      auto space = confLine.find(' ');
      if (std::string::npos == space) {
        std::cerr << "[ERROR] Invalid parameter in .conf file at line " << lineNum << "\n";
        throw std::runtime_error("Config file: Invalid synatx");
      }
      std::string key = confLine.substr(0, space);
      std::string val = confLine.substr(space + 1, confLine.size() - space);

      if ("InputUdpPort" == key) {
        this->inputUdpPort = std::stoi(val);
      } else if ("MqttUrl" == key) {
        this->mqttUrl = val;
      } else if ("MqttTopic" == key) {
        this->mqttTopic = val;
      } else if ("MqttClientID" == key) {
        this->mqttClientID = val;
      } else if ("MqttUsername" == key) {
        this->mqttUsername = val;
      } else if ("MqttPassword" == key) {
        this->mqttPassword = val;

      } else if ("MqttVersion" == key) {
        this->mqttVersion_str = val;
        if ("default" == val) {
          this->mqttVersion = MQTTVERSION_DEFAULT;
        } else if ("3.1" == val) {
          this->mqttVersion = MQTTVERSION_3_1;
        } else if ("3.1.1" == val) {
          this->mqttVersion = MQTTVERSION_3_1_1;
        } else if ("5" == val) {
          this->mqttVersion = MQTTVERSION_5;
        } else {
          std::cerr << "[ERROR] Invalid value for MqttVersion\n";
          throw std::runtime_error("Config file: Invalid synatx");
        }
      } else if ("MqttQosLevel" == key) {
        this->mqttQosLevel = std::stoi(val);
      } else if ("MqttKeepAliveInterval" == key) {
        this->mqttKeepAliveInterval = std::stoi(val);
      } else if ("MqttRetryInterval" == key) {
        this->mqttRetryInterval = std::stoi(val);
      } else if ("MqttConnectionTimeout" == key) {
        this->mqttConnectionTimeout = std::stoi(val);

      } else if ("MqttSslEnableServerCertAuth" == key) {
        this->mqttSslEnableServerCertAuth = std::stoi(val);
      } else if ("MqttSslVersion" == key) {
        this->mqttSslVersion_str = val;
        if ("default" == val) {
          this->mqttSslVersion = MQTT_SSL_VERSION_DEFAULT;
        } else if ("1.0" == val) {
          this->mqttSslVersion = MQTT_SSL_VERSION_TLS_1_0;
        } else if ("1.1" == val) {
          this->mqttSslVersion = MQTT_SSL_VERSION_TLS_1_0;
        } else if ("1.2" == val) {
          this->mqttSslVersion = MQTT_SSL_VERSION_TLS_1_0;
        } else {
          std::cerr << "[ERROR] Invalid value for MqttSslVersion\n";
          throw std::runtime_error("Config file: Invalid synatx");
        }
      } else if ("MqttSslVerify" == key) {
        this->mqttSslVerify = std::stoi(val);
      } else if ("MqttSslTrustStore" == key) {
        this->mqttSslTrustStore = val;
      } else if ("MqttSslKeyStore" == key) {
        this->mqttSslKeyStore = val;
      } else if ("MqttSslPrivateKey" == key) {
        this->mqttSslPrivateKey = val;
      } else if ("MqttSslPrivateKeyPasswd" == key) {
        this->mqttSslPrivateKeyPasswd = val;
      }
    }

    // check configuration
    bool returnValue = true;
    if (this->mqttUrl.empty()) {
      std::cerr << "[ERROR] MqttUrl must be set in the configuration file\n";
      returnValue = false;
    }
    if (this->mqttTopic.empty()) {
      std::cerr << "[ERROR] MqttTopic must be set in the configuration file\n";
      returnValue = false;
    }
    if (this->mqttClientID.empty()) {
      std::cerr << "[ERROR] MqttClientID must be set in the configuration file\n";
      returnValue = false;
    }

    if (!this->mqttUsername.empty() && this->mqttPassword.empty()) {
      std::cerr << "[ERROR] MqttPassword must be set when a username is given\n";
      returnValue = false;
    }

    return returnValue;
  }

  void printConfig() const {
    std::cout << "Configuration:\n";
    std::cout << "- Input UDP Port:       " << this->inputUdpPort << "\n";
    std::cout << "- MQTT URL:             " << this->mqttUrl << "\n";
    std::cout << "- MQTT Topic:           " << this->mqttTopic << "\n";
    std::cout << "- MQTT Client ID:       " << this->mqttClientID << "\n";
    if (!this->mqttUsername.empty()) {
      std::cout << "- MQTT User Name:       " << this->mqttUsername << "\n";
      std::cout << "- MQTT Password:        " << this->mqttPassword << "\n";
    }

    std::cout << "\n";
    std::cout << "- MQTT Version:         " << this->mqttVersion_str << "\n";
    std::cout << "- MQTT QOS Level:       " << this->mqttQosLevel << "\n";
    std::cout << "- MQTT Keep Alive Int.: " << this->mqttKeepAliveInterval << "\n";
    std::cout << "- MQTT Retry Int.:      " << this->mqttRetryInterval << "\n";

    std::cout << "- TLS Server Cert Auth: " << this->mqttSslEnableServerCertAuth << "\n";
    std::cout << "- TLS Version:          " << this->mqttSslVersion_str << "\n";
    std::cout << "- TLS Verify:           " << this->mqttSslVerify << "\n";
    if (!this->mqttSslTrustStore.empty()) {
      std::cout << "- TLS Trust Store:      " << this->mqttSslTrustStore << "\n";
    }
    if (!this->mqttSslKeyStore.empty()) {
      std::cout << "- TLS Key Store:        " << this->mqttSslKeyStore << "\n";
    }
    if (!this->mqttSslPrivateKey.empty()) {
      std::cout << "- TLS Private Key:      " << this->mqttSslPrivateKey << "\n";
      std::cout << "- TLS Priv. Key Passwd: " << this->mqttSslPrivateKeyPasswd << "\n";
    }

    std::cout << "\n";
  }

  void printUsageShort() const {
    std::cout << "usage: " << this->applicationName << " ";
    std::cout << "[-h] ";
    std::cout << "[-v] ";
    std::cout << "[-c=FILE]\n";
  }

  void printUsage() const {
    printUsageShort();
    std::cout << "\n";
    std::cout << "optional arguments:\n";
    std::cout << "  -h,          show this help message and exit\n";
    std::cout << "  -v,          increase output verbosity\n";
    std::cout << "  -c=FILE,     path to config file (default: " CONF_FILE "\n";
  }

private:
  std::string applicationName;

  std::string static trim(const std::string& str, const std::string& whitespace = " \t") {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos) {
      return "";  // no content
    }

    const auto strEnd   = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
  }

  std::string static trimComment(const std::string& str) {
    const auto strEnd = str.find_first_of('#');
    return str.substr(0, strEnd);
  }
};

/**
 * @brief Singal handler for gracefull shutdown (SIGINT, SIGTERM)
 */
void signalHandlerINT(int signum) {
  std::cout << "Interrupt signal (" << signum << ") received.\n";

  // TODO: cleanup and close up stuff here

  exit(signum);
}

int main(int argc, char* argv[]) {
  std::cout << "UDP MQTT Gateway, Version " << GIT_VERSION_TAG << "\n";

  signal(SIGINT, signalHandlerINT);
  signal(SIGTERM, signalHandlerINT);

  // parse CLI options and .conf file
  AppOptions options(argc, argv);
  std::cout << "Using configuration file: " << options.confPath << "\n\n";

  try {
    if (!options.parseConfFile()) {
      std::cout << "[INFO ] Exiting, because of invalid configuration\n";
      exit(EXIT_FAILURE);
    }
  } catch (const std::invalid_argument& e) {
    std::cout << "[INFO ] Exiting, because of an error parsing configuration\n";
    exit(EXIT_FAILURE);
  } catch (const std::runtime_error& e) {
    std::cout << "[INFO ] Exiting, because of an error parsing configuration\n";
    exit(EXIT_FAILURE);
  }

  if (options.verbosity >= 1) {
    options.printConfig();
  }

  //
  // SETUP
  //
  char msgBuffer[UDP_BUFFER_SIZE];  // NOLINT(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)

  // open an UDP socket
  struct sockaddr_in udpServerAddr {};
  struct sockaddr_in udpClientAddr {};
  int                sockfd{};

  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    std::cerr << "[ERROR] Could not create IPv4 UDP socket\n";
    exit(EXIT_FAILURE);
  }

  udpServerAddr.sin_family      = AF_INET;  // IPv4
  udpServerAddr.sin_addr.s_addr = INADDR_ANY;
  udpServerAddr.sin_port        = htons(options.inputUdpPort);

  auto retVal =
      bind(sockfd,
           reinterpret_cast<struct sockaddr*>(&udpServerAddr),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
           sizeof(udpServerAddr));
  if (0 > retVal) {
    std::cerr << "[ERROR] Could not bind UDP socket to port " << options.inputUdpPort << "\n";
    exit(EXIT_FAILURE);
  }

  if (options.verbosity >= 1) {
    std::cout << "[INFO ] Successfully opened UDP port\n";
  }

  // connect to MQTT
  MQTTClient_connectOptions mqttConnOpts = MQTTClient_connectOptions_initializer;
  MQTTClient_SSLOptions     mqttSslOpts  = MQTTClient_SSLOptions_initializer;
  MQTTClient                mqttClient{};

  MQTTClient_create(&mqttClient, options.mqttUrl.c_str(), options.mqttClientID.c_str(), MQTTCLIENT_PERSISTENCE_NONE,
                    nullptr);
  mqttConnOpts.keepAliveInterval = options.mqttKeepAliveInterval;
  mqttConnOpts.cleansession      = 1;
  mqttConnOpts.connectTimeout    = options.mqttConnectionTimeout;
  mqttConnOpts.retryInterval     = options.mqttRetryInterval;
  if (!options.mqttUsername.empty()) {
    mqttConnOpts.username = options.mqttUsername.c_str();
    mqttConnOpts.password = options.mqttPassword.c_str();
  }
  mqttConnOpts.MQTTVersion = options.mqttVersion;

  mqttSslOpts.enableServerCertAuth = options.mqttSslEnableServerCertAuth;
  mqttSslOpts.sslVersion           = options.mqttSslVersion;
  mqttSslOpts.verify               = options.mqttSslVerify;
  if (!options.mqttSslTrustStore.empty()) {
    mqttSslOpts.trustStore = options.mqttSslTrustStore.c_str();
  }
  if (!options.mqttSslKeyStore.empty()) {
    mqttSslOpts.keyStore = options.mqttSslKeyStore.c_str();
  }
  if (!options.mqttSslPrivateKey.empty()) {
    mqttSslOpts.privateKey = options.mqttSslPrivateKey.c_str();
  }
  if (!options.mqttSslPrivateKeyPasswd.empty()) {
    mqttSslOpts.privateKeyPassword = options.mqttSslPrivateKeyPasswd.c_str();
  }

  mqttConnOpts.ssl = &mqttSslOpts;

  int mqttRC = MQTTClient_connect(mqttClient, &mqttConnOpts);
  if (MQTTCLIENT_SUCCESS != mqttRC) {
    std::cerr << "[ERROR] Failed to connect to MQTT broker, error " << mqttRC << ":";
    switch (mqttRC) {
    case -1:  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers
      std::cerr << "Unacceptable protocol version\n";
      break;
    case -2:  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers
      std::cerr << "Identifier rejected\n";
      break;
    case -3:  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers
      std::cerr << "Server unavailabl\n";
      break;
    case -4:  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers
      std::cerr << "Bad user name or password\n";
      break;
    case -5:  // NOLINT(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      std::cerr << "Not authorized\n";
      break;
    default:
      std::cerr << "Unknown error code\n";
      break;
    }
    exit(EXIT_FAILURE);
  }

  if (options.verbosity >= 1) {
    std::cout << "[INFO ] Successfully connected to MQTT broker\n";
  }

  //
  // MAIN LOOP
  //
  while (true) {
    // wait for new UDP packet
    socklen_t len    = sizeof(udpClientAddr);
    int       msgLen = recvfrom(
        sockfd, &msgBuffer[0], UDP_BUFFER_SIZE, MSG_WAITALL,
        reinterpret_cast<struct sockaddr*>(&udpClientAddr),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        &len);

    if (options.verbosity >= 2) {
      std::cout << "[DEBUG] Got a new message\n";
    }

    // publish message to MQTT
    MQTTClient_deliveryToken token{};
    MQTTClient_message       pubmsg = MQTTClient_message_initializer;

    pubmsg.payload    = static_cast<void*>(msgBuffer);
    pubmsg.payloadlen = msgLen;
    pubmsg.qos        = options.mqttQosLevel;
    pubmsg.retained   = 0;
    MQTTClient_publishMessage(mqttClient, options.mqttTopic.c_str(), &pubmsg, &token);

    mqttRC = MQTTClient_waitForCompletion(mqttClient, token, options.mqttConnectionTimeout);
    if (MQTTCLIENT_SUCCESS != mqttRC) {
      std::cout << "[ERROR] Failed to publish MQTT message, error " << mqttRC << "\n";
    }

    if (options.verbosity >= 2) {
      std::cout << "[DEBUG] Successfully published message to MQTT\n";
    }
  }

  return EXIT_SUCCESS;
}