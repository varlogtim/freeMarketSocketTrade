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

/*
 * OK, more information about the client/server architecture...
 * The accept() and connect() functions both block.
 * The server must accept() and the clients must connect()
 * 
 * We could have the "master" facilitate all the "trades"
 * but I would like each of them to have a connection to
 * the other servers.
 *
 * First: the connection from the parent to all the child:
 *
 * So, in order to do this, we need to spawn the children
 * and have each of them call connect() and wait until the
 * parent calls accept(). We need to make sure there are
 * enough backlog slots configured for this.
 *
 * Hrmm... yeah ... I should just get the master/broker
 * model working first.
 *
 */


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
    int numPeers;
    int socketFd;
};


bool serverSetup(const char *socketPath, int numPeers, UnixDomainSocket *uds) {
    // TODO: See comment at top of clientConnect()
    memset(&uds->addr, '\0', sizeof(struct sockaddr_un));

    uds->socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    uds->addrsize = sizeof(struct sockaddr_un);
    uds->addr.sun_family = AF_UNIX;

    strcpy(uds->socketPath, socketPath);
    strncpy(uds->addr.sun_path, uds->socketPath, sizeof(uds->addr.sun_path) - 1);

    int status;
    status = bind(
            uds->socketFd, (struct sockaddr *) &uds->addr, uds->addrsize);
    if (status == -1) {
        std::cerr << "Failed to bind to address: " 
            << std::strerror(errno) << std::endl;
        return false;
    } else {
        std::cout << "Bind Success" << std::endl;
    }
    
    int backlog = 5; // XXX needed? maybe numPeers
    status = listen(uds->socketFd, backlog);
    if (status == -1) {
        std::cerr << "Failed to listen to address: " 
            << std::strerror(errno) << std::endl;
        return false;
    }

    std::cout << "Server Listen Successful: "
        << "Socket FD: " << uds->socketFd << " "
        << "Socket Path: " << uds->addr.sun_path << std::endl;

    return true;

}

bool clientConnect(const char *socketPath, UnixDomainSocket *uds) {
    // Note to self: We might be able to reuse this pattern from
    // the serverSetup() ... I think there was a memset issue which
    // caused my problem yesterday. Yeah, actually, if I made the
    // UnixDomainSocket a more generic structure and created it
    // outside of this function, it wouldn't matter if it were
    // in the UNIX or INET communication domain.
    memset(&uds->addr, '\0', sizeof(struct sockaddr_un));

    uds->socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    uds->addrsize = sizeof(struct sockaddr_un);
    uds->addr.sun_family = AF_UNIX;

    strcpy(uds->socketPath, socketPath);
    strncpy(uds->addr.sun_path, uds->socketPath, sizeof(uds->addr.sun_path) - 1);

    int status = connect(
            uds->socketFd, (struct sockaddr *) &uds->addr, uds->addrsize);

    if (status == -1) {
        std::cout << "Client Connection Failed: " 
            << std::strerror(errno) << std::endl;
        return false;
    }

    std::cout << "Client Connection Successful: "
        << "Socket FD: " << uds->socketFd << " "
        << "Socket Path: " << uds->addr.sun_path << std::endl;
    // this blocks too
    return true;
}

int clientProcess(const char *socketPath)
{
    // TODO: better error handling
    UnixDomainSocket *uds = nullptr;
    uds = (UnixDomainSocket *) malloc(sizeof(UnixDomainSocket));
    if (!uds) return 1;

    bool status;
    status = clientConnect(socketPath, uds);
    if (!status) return 1;
    
    std::string data("Hello, yo!");

    int writeBytes;
    writeBytes = write(uds->socketFd, data.c_str(), data.size());
    std::cout << "writeBytes: " << writeBytes << std::endl;

    return 0;
}

int masterProcess(const char *socketPath, int numPeers)
{
    UnixDomainSocket *uds = nullptr;
    uds = (UnixDomainSocket *) malloc(sizeof(UnixDomainSocket));
    if (!uds) return 1;

    bool status;
    status = serverSetup(socketPath, numPeers, uds);
    if (!status) return 1;

    int buffSize = 1000;
    char buff[buffSize];

    /*
    */

    // So ... it turns out that when we accept a connection, we create
    // another file descriptor which is what we read from. I suppose this
    // makes sense as streams are bidirectional. The question is, when we
    // write from the server to the client, do we write to cfd or sfd?
    // - Hrm.. docs say accept "optionally returns address of peer socket"
    int cfd = accept(uds->socketFd, nullptr, nullptr);
            //uds->socketFd, (struct sockaddr *) &uds->addr, &uds->addrsize);

    // so... accept overwrites the uds->addr struct on connect, if specified.

    // blocks until connect.
    if (cfd == -1) {
        std::cerr << "accept failed: " << std::strerror(errno) << std::endl;
        return false;
    } else {
        std::cout << "Server accept file descriptor: " << cfd << std::endl;
    }
    int bytesRead;

    std::string data("");
    while((bytesRead = read(cfd, buff, buffSize)) > 0) {
        data.append(std::string(buff), 0, bytesRead);
    }
    if (bytesRead == -1) {
        std::cerr << "read failed: " << std::strerror(errno) << std::endl;
    }
    std::cout << "Data read: '" << data << "'" << std::endl;
    return 0;
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

    /*
    Trader person1 = Trader(1, 100);

    for (int ii = *numTrades; ii--; ) {
        person1.buy(2, 13);
    }
    */

    const char *socketPath = "/tmp/test.sock";

    pid_t pid = fork();
    if (pid > 0) {
        // parent
        masterProcess(socketPath, *numTraders);
    } else {
        if (pid == 0) {
            // child
            clientProcess(socketPath);
            exit(0);
        } else {
            // error
            std::cerr << "Error spawning child" << std::endl;
        }
    }

    unlink(socketPath);
    return 0;
}



