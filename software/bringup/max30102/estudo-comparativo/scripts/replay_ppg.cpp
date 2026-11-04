#include "core/PpgProcessor.h"
#include <cstdio>
#include <fstream>
#include <string>
// le "leitura_s,n,red,ir" e imprime FC,SpO2,FR a cada 1 s (25 amostras)
int main(int argc, char** argv)
{
    std::ifstream in(argv[1]); std::string line; PpgProcessor p; int n = 0;
    std::getline(in, line);
    while(std::getline(in, line))
    {
        double t; unsigned batch, red, ir;
        if(sscanf(line.c_str(), "%lf,%u,%u,%u", &t, &batch, &red, &ir) != 4) continue;
        p.addSample({red, ir}); ++n;
        if(n % 25 == 0)
            printf("%.0f,%d,%d,%d,%d\n", n / 25.0, p.hasFingerContact(), p.heartRate().value_or(0),
                   p.oxygenSaturation().value_or(0), p.respiratoryRate().value_or(0));
    }
}
