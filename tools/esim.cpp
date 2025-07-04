#include "esim.h"

double fs(double d, int wFactor, int nObjects) {
    if (wFactor == INT_MIN)
        return d/nObjects;
    return pow(wFactor, nObjects - d);
}
double fd(double d, int wFactor, int nObjects) {
    if (wFactor == INT_MIN)
        return 1-(d-nObjects % 2)/nObjects;
    return pow(wFactor, nObjects % 2 - d);
}
Counters calculateCounters(VectorXd& cTotal, int nObjects, Threshold cThreshold, int wFactor){
    int a=0;
    int d=0;
    int dis=0;
    double wa=0;
    double wd=0;
    double wDis=0;

    for(double w : cTotal){
        int diff = 2 * w - nObjects;
        if(diff > cThreshold.value){
            ++a;
            wa += fs(w, wFactor, nObjects);
        } else if (diff < cThreshold.value){
            ++d;
            wd += fs(w, wFactor, nObjects);
        }
        if(abs(diff) <= cThreshold.value){
            ++dis;
            wDis += fd(w, wFactor, nObjects);
        }
    }

    return Counters(a, wa, d, wd, dis, wDis);
}

Indices genSimIdx(VectorXd& cTotal, int nObjects, Threshold cThreshold, int wt) {
    Counters cnt = calculateCounters(cTotal, nObjects, cThreshold, wt);
    double bub = pow(cnt.wa * cnt.wd, 0.5 + cnt.wa) / pow(cnt.a + cnt.d, 0.5 + cnt.a + cnt.totalDis);
    double fai = (cnt.wa + 0.5 * cnt.wd) / cnt.p;
    double gle = 2 * cnt.wa / (2 * cnt.a + cnt.totalDis);
    double ja = 3 * cnt.wa / (3 * cnt.a + cnt.totalDis);
    double jt = cnt.wa / (cnt.a + cnt.totalDis);
    double rt = cnt.totalWsim / (cnt.p + cnt.totalDis);
    double rr = cnt.wa / cnt.p;
    double sm = cnt.totalWsim / cnt.p;
    double ss1 = cnt.wa / (cnt.a + 2 * cnt.totalDis);
    double ss2 = 2 * cnt.totalWsim / (cnt.p + cnt.totalSim);
    return Indices(bub, fai, gle, ja, jt, rt, rr, sm , ss1, ss2);
}
