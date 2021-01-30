#include <bits/stdc++.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCK_BACKLOG 10

/*
 * Plan
 *
 * Spawn a number of processes which will act as free individuals all starting
 * with the exact same amount of money. These processes will then "trade" a
 * random amount of money (really just one person gives) depending upon a coin
 * flip.
 *
 * Each process is both a seller and a buyer. They need to be aware of all the
 * other processes. A process will contact a random other process with a sell
 * intention. If that process wins the coin flip, a certain amount of money
 * will be transfered from the buyer process to the seller.
 *
 * All processes should log their transactions.
 *
 * We need a stop condition ... So, will do number of iterations.
 * We could also add signal handling as well. Will be neato.
 */

class Logger
{
  public:
    // not sure where to get base directory.
    Logger(int traderId, std::string baseLogPath);
    ~Logger() = default;
    
    void log(std::string msg);
    void flush(); // think about this later.

  private:
    int traderId;
    std::string baseLogPath;
};

Logger::Logger(int traderId, std::string baseLogPath) {
    baseLogPath = baseLogPath;
}

void
Logger::log(std::string msg) {

}

void
Logger::flush() {}


/*
 * Trader Class
 */

class Trader
{
  public:
    // Can't figure out how to pass a point to a class instance
    // as an argument to a function :/
    Trader(int id, int money);
    ~Trader() = default;
    bool buy(int sellerId, int amount);
    bool shouldBuy();
    void info(std::string msg); // poor name for this.
  private:
    int id_;
    int money_;
};


Trader::Trader(int id, int money)
{
    id_ = id;
    money_ = money;
}

void
Trader::info(std::string msg) {
    boost::posix_time::ptime time;
    time = boost::posix_time::microsec_clock::universal_time();

    std::cout 
        << boost::posix_time::to_iso_extended_string(time) << "Z: "
        << std::setfill('0') << std::setw(6) << id_ 
        << ": " << msg << std::endl;
}

bool
Trader::shouldBuy() {
    int num = rand() % 100;
    if (num >= 50) {
        return true;
    }
    return false;
}


// TODO: this should be JSON formatted. Need a function for formatting
// the record of proposed transaction. Also, function name should be different.
bool
Trader::buy(int sellerId, int amount)
{
    bool ret = false;
    std::stringstream ss;

    if (amount <= money_) {
        if (shouldBuy()) {
            money_ -= amount;
            ss << "Buy for: " << amount
                << ". Money left: " << money_; 

            ret = true;
        } else {
            ss << "Declined buy for: " << amount << " from " 
                << std::setfill('0') << std::setw(6) << sellerId
                << ". Money left: " << money_;

            ret = false;
        }
    } else {
        ss << "Cannot afford buy for: " << amount
            << ". Money left: " << int(money_);

        ret = false;
    }
    info(ss.str());

    return ret;
}


bool makeTraders(int numTraders, int *traderIds) {
    return true;
}

void usage() {
    // FIXME - this needs work.
    std::cout
        << "Purpose:" << std::endl
        << " Launch processes representing 'Traders' which start "
        << "with a particular amount of money and trade with each "
        << "other ..."
        << std::endl << std::endl
        << "Usage:" << std::endl
        << " ./freeMarket <num_traders> <num_trades>"
        << std::endl << std::endl;
}

bool parseArgs(int argc, char *argv[], int *numTraders, int *numTrades)
{
    if (argc != 3) {
        usage();
        std::cerr << "Incorrect number of arguments" << std::endl;
        return false;
    }

    *numTraders = atoi(argv[1]);
    *numTrades = atoi(argv[2]);

    if (numTraders <= 0 || numTrades <= 0) {
        usage();
        std::cerr
            << "<num_traders> and <num_trades> must "
            << "be integers greater than zero." << std::endl;
        return false;
    }
    
    return true;
}

class UnixDomainSocket_DERP {
  public:
    UnixDomainSocket_DERP(const char *socketPath);
    ~UnixDomainSocket_DERP() = default;
    bool prepareSocket();
  private:
    struct sockaddr_un addr_;
    socklen_t addrsize_;
    char *socketPath_;
    int socketFd_;
};

// Thinking this should be a class
struct UnixDomainSocket {
    char socketPath[PATH_MAX];
    struct sockaddr_un addr;
    socklen_t addrsize;
    int socketFd;
};

bool prepareUnixDomainSocket(const char *socketPath, UnixDomainSocket *uds) {
    strcpy(uds->socketPath, socketPath);

    uds->socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    uds->addrsize = sizeof(struct sockaddr_un);

    memset(&uds->addr, '\0', uds->addrsize);

    uds->addr.sun_family = AF_UNIX;
    strncpy(uds->addr.sun_path, uds->socketPath, sizeof(uds->addr.sun_path) - 1);

    return true;
}

bool serverSetup(UnixDomainSocket *uds) {
    //std::cout << "Server waiting 10 seconds" << std::endl;
    //sleep(10);
    int bindReturn = bind(
            uds->socketFd, (struct sockaddr *) &uds->addr, uds->addrsize);
    if (bindReturn == -1) {
        std::cerr << "Failed to bind to address: " 
            << std::strerror(errno) << std::endl;
        return false;
    } else {
        std::cout << "Bind Success" << std::endl;
    }
    
    int listenRet = listen(uds->socketFd, 1);
    if (listenRet == -1) {
        std::cerr << "Failed to listen to address: " 
            << std::strerror(errno) << std::endl;
        return false;
    } else {
        std::cout << "Listen Success" << std::endl;
    }

    int acceptRet = accept(
            uds->socketFd, (struct sockaddr *) &uds->addr, &uds->addrsize);
    if (acceptRet == -1) {
        std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
        return false;
    } else {
        std::cout << "Accept Success" << std::endl;
    }
    return true;
}

bool clientConnect(UnixDomainSocket *uds) {
    int numTries = 1;
    std::cout << "uds->addr: " << uds->addr.sun_path << std::endl;
    for (int ii = 0; ii < numTries; ii++) {
        int connectRet = connect(
                uds->socketFd, (struct sockaddr *) &uds->addr, (socklen_t) uds->addrsize);
        if (connectRet == -1) {
            std::cout << "connect " << ii << " failed: " 
                << std::strerror(errno) << ": Retrying ..." << std::endl;
        } else {
            std::cout << "Connection successful" << std::endl;
            break;
        }
        sleep(1);
    }
    // this blocks too
    return true;
}

int main(int argc, char *argv[]) {
    // this is strange, move this to a struct.
    int *numTraders = nullptr;
    int *numTrades = nullptr;
    numTraders = (int *) malloc(sizeof(numTraders));
    numTrades = (int *) malloc(sizeof(numTrades));

    bool parsedArgs = parseArgs(argc, argv, numTraders, numTrades);

    if (!parsedArgs || !numTraders || !numTrades) {
        exit(1);
    }

    Trader person1 = Trader(1, 100);

    for (int ii = *numTrades; ii--; ) {
        person1.buy(2, 13);
    }

    UnixDomainSocket *serverUds = nullptr;
    UnixDomainSocket *clientUds = nullptr;
    serverUds = (UnixDomainSocket *) malloc(sizeof(UnixDomainSocket));
    clientUds = (UnixDomainSocket *) malloc(sizeof(UnixDomainSocket));
    const char *socketPath = "/tmp/test.sock";


    pid_t pid = fork();
    std::cout << "PID PID: " << pid << std::endl;
    if (pid > 0) {
        // parent
        if(!prepareUnixDomainSocket(socketPath, serverUds)) {
            std::cerr << "Failed to prepare server socket" << std::endl;
        }
        serverSetup(serverUds);
    } else {
        if (pid == 0) {
            // child
            if(!prepareUnixDomainSocket(socketPath, clientUds)) {
                std::cerr << "Failed to prepare client socket" << std::endl;
            }
            clientConnect(clientUds);
            exit(0);
        } else {
            // error
            std::cerr << "Error spawning child" << std::endl;
        }
    }
    return 0;
}



