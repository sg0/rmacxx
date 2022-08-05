#include <iostream>
#include <vector>
#include <tuple>

// Return whether an overlap has occurred, and if so then return the starts and ends of it
inline std::tuple<int, std::vector<int>, std::vector<int>> check_overlap(std::vector<int> wlo, std::vector<int> whi, std::vector<int> plo, std::vector<int> phi) {
    std::vector<int> starts;
    std::vector<int> ends;

    if (wlo.empty() || wlo.size() == 0) {
        return std::make_tuple(false, starts, ends);
    }
   
    int overlap = 2; //0 = no overlap, 1 = partial overlap, 2 = fully contained
    for (int i = 0; i < wlo.size(); i++) { //check every dimension for an overlap

        //check if process fully encloses the window
        if (plo[i] <= wlo[i] && whi[i] <= phi[i]) {
            starts.push_back(wlo[i]);
            ends.push_back(whi[i]);
            continue;
        }

        //check if window fully encloses the process
        if (wlo[i] <= plo[i] && phi[i] <= whi[i]) {
            starts.push_back(plo[i]);
            ends.push_back(phi[i]);
            overlap = 1;
            continue;
        }

        //check for partial overlap
        if (wlo[i] <= plo[i] <= whi[i]) {
            starts.push_back(plo[i]);
            ends.push_back(whi[i]);
            overlap = 1;
            continue;
        }
        if (wlo[i] <= phi[i] <= whi[i]) {
            starts.push_back(wlo[i]);
            ends.push_back(phi[i]);
            overlap = 1;
            continue;
        }

        //if none of the conditions were met, then an overlap has not occurred
        overlap = 0;
        break;
    }
    return std::make_tuple(overlap, starts, ends);
}


// Given the size of the window and the lows and highs of the processes, return whether the window has no gaps
bool check_gaps(std::vector<int> wsize, std::vector<std::vector<int>> plos, std::vector<std::vector<int>> phis) {
    
    //set up vectors of window lows and highs
    std::vector<std::vector<int>> wlos, whis;

    //add the overall window
    std::vector<int> all_zeroes(wsize.size());
    wlos.push_back(all_zeroes);
    whis.push_back(wsize);

    //run loop once per process
    for (int p = 0; p < plos.size(); p++) {

        //create new vectors for new windows which replace wlos and whis at the end of this iteration
        std::vector<std::vector<int>> nwlos, nwhis;
        
        //pick a window to split
        for (int w = 0; w < wlos.size(); w++) {
        
            //check the overlap status between the process and the window; result[0]: 0 = no overlap, 1 = partial overlap, 2 = window is fully contained
            std::tuple<int, std::vector<int>, std::vector<int>> result = check_overlap(wlos[w], whis[w], plos[p], phis[p]);
            int res = std::get<0>(result);
            
            //if no overlap, then add the window again
            if (res == 0) {
                nwlos.push_back(wlos[w]);
                nwhis.push_back(whis[w]);
                continue;
            }

            //if partial overlap, then go through the splitting process
            if (res == 1) {
                
                //setup the storage vectors and the cur window
                std::vector<std::vector<int>> lwlos, lwhis;
                std::vector<int> curlo = wlos[w];
                std::vector<int> curhi = whis[w];

                //store the bounds of the process chunk we're looking at
                std::vector<int> plo = std::get<1>(result);
                std::vector<int> phi = std::get<2>(result);

                //split the window in every dimension
                for (int d = 0; d < wsize.size(); d++) {
                    
                    //check to see if window extends before the process
                    if (curlo[d] < plo[d]) {
                        
                        //generate a new window to append to lwlo/hi
                        std::vector<int> tlo(curlo);
                        std::vector<int> thi(curhi);
                        thi[d] = plo[d] - 1;
                        lwlos.push_back(tlo);
                        lwhis.push_back(thi);

                        //resize cur
                        curlo[d] = plo[d];
                    }

                    //check to see if window extends after the process
                    if (curhi[d] > phi[d]) {
                        
                        //generate a new window to append to lwlo/hi
                        std::vector<int> tlo(curlo);
                        std::vector<int> thi(curhi);
                        tlo[d] = phi[d] + 1;
                        lwlos.push_back(tlo);
                        lwhis.push_back(thi);

                        //resize cur
                        curhi[d] = phi[d];
                    }
                }

                //append the newly split windows to the list of new windows
                nwlos.insert(nwlos.end(), lwlos.begin(), lwlos.end());
                nwhis.insert(nwhis.end(), lwhis.begin(), lwhis.end());

            }

            //if fully enclosed, then just delete this window
            if (res == 2) {
                continue;
            }

        }

        //update the current list of windows
        wlos = nwlos;
        whis = nwhis;

    }

    return wlos.size() == 0;
}
