#include <iostream>
#include <vector>
#include <tuple>

// Prints a vector in a readable format
template<typename T> std::string print_vector(std::vector<T> vec) {
    std::cout<<"[ ";
    int N = vec.size();
    for (int i = 0; i < N; i++) {
        std::cout<<(vec[i])<<" ";
    }
    std::cout<<"]"<<std::endl;
    return "";
}

// Return whether an overlap has occurred, and if so then return the starts and ends of it
inline std::tuple<int, std::vector<int>, std::vector<int>> check_overlap(std::vector<int> wlo, std::vector<int> whi, std::vector<int> plo, std::vector<int> phi) {
    std::vector<int> starts;
    std::vector<int> ends;

    //std::cout<<" - checking overlap"<<std::endl;

    if (wlo.empty() || wlo.size() == 0) {
        return std::make_tuple(false, starts, ends);
    }

    //if all parameters are not the same size
    if (!((wlo.size() == whi.size()) && (plo.size() == phi.size()) && (wlo.size() == plo.size()))) {
        //std::cout<<"ERROR: Parameters not the same size"<<std::endl;
        return std::make_tuple(false, starts, ends);
    }
   
    int overlap = 2; //0 = no overlap, 1 = partial overlap, 2 = fully contained
    for (int i = 0; i < wlo.size(); i++) { //check every dimension for an overlap

        //std::cout<<" - checking first condition"<<std::endl;
        //check if process fully encloses the window
        if (plo[i] <= wlo[i] && whi[i] <= phi[i]) {
            //std::cout<<" - in first condition"<<std::endl;
            starts.push_back(wlo[i]);
            ends.push_back(whi[i]);
            //std::cout<<" - continuing"<<std::endl;
            continue;
        }

        //std::cout<<" - checking second condition"<<std::endl;
        //check if window fully encloses the process
        if (wlo[i] <= plo[i] && phi[i] <= whi[i]) {
            //std::cout<<" - in second condition"<<std::endl;
            starts.push_back(plo[i]);
            ends.push_back(phi[i]);
            overlap = 1;
            //std::cout<<" - continuing"<<std::endl;
            continue;
        }

        //std::cout<<" - checking third condition"<<std::endl;
        //check for partial overlap
        if (wlo[i] <= plo[i] && plo[i] <= whi[i]) {
            //std::cout<<" - in third condition"<<std::endl;
            starts.push_back(plo[i]);
            ends.push_back(whi[i]);
            overlap = 1;
            //std::cout<<" - continuing"<<std::endl;
            continue;
        }
        //std::cout<<" - checking fourth condition"<<std::endl;
        if (wlo[i] <= phi[i] && phi[i] <= whi[i]) {
            //std::cout<<" - in fourth condition"<<std::endl;
            starts.push_back(wlo[i]);
            ends.push_back(phi[i]);
            overlap = 1;
            //std::cout<<" - continuing"<<std::endl;
            continue;
        }
        //std::cout<<" - no condition met"<<std::endl;
        //if none of the conditions were met, then an overlap has not occurred
        overlap = 0;
        break;
    }

    //std::cout<<" - finished overlap"<<std::endl;
    return std::make_tuple(overlap, starts, ends);
}


// Given the size of the window and the lows and highs of the processes, returns true if the window has no gaps
bool check_gaps(std::vector<int> wsize, std::vector<std::vector<int>> plos, std::vector<std::vector<int>> phis) {
    
    //size checks
    if (plos.size() != phis.size()) {
        std::cout<<"ERROR CHECK_GAPS: Number of processes not consistent"<<std::endl;
        return false;
    }
    int dims = wsize.size(); //the number of dimensions, and everything should respect this number
    for (int i = 0; i < plos.size(); i++) {
        if (!(plos[i].size() == dims && phis[i].size() == dims)) {
            std::cout<<"ERROR CHECK_GAPS: Number of dimensions not consistent"<<std::endl;
            //std::cout<<"plos["<<i<<"]: "<<plos[i].size()<<std::endl;
            //std::cout<<"phis["<<i<<"]: "<<phis[i].size()<<std::endl;
            return false;
        }
    }

    //set up vectors of window lows and highs
    std::vector<std::vector<int>> wlos, whis;

    //add the overall window
    std::vector<int> all_zeroes(dims);
    std::vector<int> nwsize(dims);
    for (int i = 0; i < dims; i++) {
        nwsize[i] = wsize[i] - 1;
    }
    wlos.push_back(all_zeroes);
    whis.push_back(nwsize);

    //std::cout<<"HERE"<<std::endl;

    //run loop once per process
    for (int p = 0; p < plos.size(); p++) {
        //std::cout<<"Process: "<<rmacxx::print_vector(plos[p])<<"      to "<<rmacxx::print_vector(phis[p])<<std::endl;
        //create new vectors for new windows which replace wlos and whis at the end of this iteration
        std::vector<std::vector<int>> nwlos, nwhis;
        
        //pick a window to split
        for (int w = 0; w < wlos.size(); w++) {
            //std::cout<<"Window: "<<rmacxx::print_vector(wlos[w])<<"     to "<<rmacxx::print_vector(whis[w])<<std::endl;
            //check the overlap status between the process and the window; result[0]: 0 = no overlap, 1 = partial overlap, 2 = window is fully contained
            std::tuple<int, std::vector<int>, std::vector<int>> result = check_overlap(wlos[w], whis[w], plos[p], phis[p]);
            int res = std::get<0>(result);
            
            //if no overlap, then add the window again
            if (res == 0) {
                //std::cout<<"no overlap"<<std::endl;
                nwlos.push_back(wlos[w]);
                nwhis.push_back(whis[w]);
                continue;
            }

            //if partial overlap, then go through the splitting process
            if (res == 1) {
                
                //std::cout<<"partial overlap"<<std::endl;
                //setup the storage vectors and the cur window
                std::vector<std::vector<int>> lwlos, lwhis;
                std::vector<int> curlo = wlos[w];
                std::vector<int> curhi = whis[w];

                //store the bounds of the process chunk we're looking at
                std::vector<int> plo = std::get<1>(result);
                std::vector<int> phi = std::get<2>(result);

                //split the window in every dimension
                for (int d = 0; d < dims; d++) {
                    //std::cout<<"Checking dimension "<<d<<std::endl;
                    //check to see if window extends before the process
                    if (curlo[d] < plo[d]) {
                        //std::cout<<"clipping before"<<std::endl;
                        //generate a new window to append to lwlo/hi
                        std::vector<int> tlo(curlo);
                        std::vector<int> thi(curhi);
                        thi[d] = plo[d] - 1;
                        lwlos.push_back(tlo);
                        lwhis.push_back(thi);
                        //std::cout<<"created: "<<rmacxx::print_vector(tlo)<<"      to "<<rmacxx::print_vector(thi)<<std::endl;

                        //resize cur
                        curlo[d] = plo[d];
                    }

                    //check to see if window extends after the process
                    if (curhi[d] > phi[d]) {
                        //std::cout<<"clipping after"<<std::endl;
                        //generate a new window to append to lwlo/hi
                        std::vector<int> tlo(curlo);
                        std::vector<int> thi(curhi);
                        tlo[d] = phi[d] + 1;
                        lwlos.push_back(tlo);
                        lwhis.push_back(thi);
                        //std::cout<<"created: "<<rmacxx::print_vector(tlo)<<"      to "<<rmacxx::print_vector(thi)<<std::endl;

                        //resize cur
                        curhi[d] = phi[d];
                    }
                }

                //std::cout<<"nwlos size: "<<nwlos.size()<<std::endl;
                //std::cout<<"lwlos size: "<<lwlos.size()<<std::endl;
                //append the newly split windows to the list of new windows
                nwlos.insert(nwlos.end(), lwlos.begin(), lwlos.end());
                nwhis.insert(nwhis.end(), lwhis.begin(), lwhis.end());
                //std::cout<<"nwlos new size: "<<nwlos.size()<<std::endl;

            }

            //if fully enclosed, then just delete this window
            if (res == 2) {
                //std::cout<<"full overlap"<<std::endl;
                continue;
            }

        }

        //update the current list of windows
        wlos = nwlos;
        whis = nwhis;

    }

    return wlos.size() == 0;
}
