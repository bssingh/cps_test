CXXFILES = cps_client.cpp

CXXFLAGS = -O3 -o cps_client
CXXFLAGS1 = -O3 -o cps_server

LIBS = -ltins -lpthread

all:
	$(CXX) $(CXXFILES) $(LIBS) $(CXXFLAGS)
	$(CXX) $(CXXFLAGS1) cps_server.cpp $(LIBS) 

clean:
	rm -f prog *.o
	rm -f cps_client cps_server 
