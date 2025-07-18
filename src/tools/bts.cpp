#include "bts.h"

/* O(N) Mean square deviation(MSD) calculation for n-ary objects.
 *  
 * Parameters:
 *  - data: A feature array of size (nSamples, nFeatures)
 *  - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems.
 * 
 * Returns: normalized MSD value
 * 
 * Reference: https://github.com/mqcomplab/MDANCE/blob/9a895e72d71fee1d1a4fad1700a806473dff2f71/src/mdance/tools/bts.py#L14
*/ 
double meanSqDev(MatrixXd& data, int nAtoms){
    Index N = data.rows();
    if (N == 1)
        return 0;
    VectorXd cSum = data.colwise().sum();
    VectorXd sqSum = data.array().square().colwise().sum();
    return msdCondensed(cSum, sqSum, N, nAtoms);
}

/* Condensed version of Mean square deviation (MSD) calculation for n-ary objects
 * 
 * Parameters:
 *  - cSum: A feature array of the column-wise sum of the data (nFeatures)
 *  - sqSum: A feature array of the column-wise sum of the squared data (nFeatures)
 *  - N: Number of data points
 *  - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems.
 * 
 * Returns: normalized MSD value.
 * 
 * Reference: https://github.com/mqcomplab/MDANCE/blob/9a895e72d71fee1d1a4fad1700a806473dff2f71/src/mdance/tools/bts.py#L54
*/
double msdCondensed(VectorXd& cSum, VectorXd& sqSum, Index N, int nAtoms){
    if (N == 1)
        return 0;
    /* The following is a step-by-step explanation of what we are returning. May need to use this instead if we run into overflow issues?
     * VectorXd meanSqSum = sqSum.array() / N;
     * VectorXd meanCSum = cSum.array() / N;
     * double msd = (meanSqSum.array() - meanCSum.array().square()).sum();
    */
    return (sqSum.array() * N - cSum.array().square()).sum() * 2.0 / (N * N * nAtoms);
}

/* O(N) Extended comparison function for n-ary objects.
 *
 * Parameters:
 * - data: A feature array which can take on multiple formats
 *    --> if (!isCondensed): an array of size (nSamples, nFeatures)
 *    --> if (isCondensed): a MatrixXd with 1 row (cSum)
 *    --> if (isCondensed): a MatrixXd with 2 rows (cSum, sqSum)
 * - isCondensed: Controls type of data (see above)
 * - mt: The metric to use when calculating distance between n objects in an array
 * - N: Number of data points.
 * - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems
 * - cThreshold: CCoincidence threshold for calculatinig extended similarity
 * - wPower: Controls the type of weight function for calculating extended similarity
 *    --> if (wPower): use fraction method
 *    --> else: use powerN method where N=wPower
 * 
 * Returns: Extended comparison value.
 * 
 * Reference: https://github.com/mqcomplab/MDANCE/blob/main/src/mdance/tools/bts.py#L96
*/
double extendedComparison(MatrixXd& data, Index N, int nAtoms, bool isCondensed, Metric mt, Threshold cThreshold, int wPower) {
    // Handle default initialization of Threshold
    if (cThreshold.type == ThresholdType::None)
        cThreshold.value = N % 2;
    // Data check
    if (isCondensed){
        if (data.rows() > 2){
            std::cerr << "Data must have at most two rows: either (cSum) or (cSum, sqSum)" << std::endl;
            exit;
        }
        VectorXd cSum = data.row(0);
        if (mt == Metric::MSD){
            VectorXd sqSum = data.row(1);
            return msdCondensed(cSum, sqSum, N, nAtoms);
        } else {
            Indices idx = genSimIdx(cSum, N, cThreshold, wPower);
            return 1 - idx.getIndex(mt);
        }
    } else {
        if (mt == Metric::MSD) {
            return meanSqDev(data, nAtoms);
        } else {
            VectorXd cSum = data.colwise().sum();
            Indices idx = genSimIdx(cSum, N, cThreshold, wPower);
            return 1 - idx.getIndex(mt);
        }
    }

}

/* O(N) Complementary similarity calculation for n-ary objects.
 *
 * Parameters:
 *  - data: A feature array of shape (nSamples, nFeatures).
 *  - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems.
 *  - mt: The metric to use when calculating distance between n objects in an array.
 * 
 * Returns: Vector (N) of complementary similarities for each object
 * 
 * Reference: https://github.com/mqcomplab/MDANCE/blob/main/src/mdance/tools/bts.py#L190
*/
VectorXd calculateCompSim(MatrixXd& data, int nAtoms, Metric mt){
    Index N = data.rows();

    MatrixXd sqData = data.array().square();
    VectorXd cSum = data.colwise().sum();
    VectorXd sqSum = sqData.colwise().sum();
    MatrixXd compC = ((-data).rowwise()+cSum.transpose()) / (N-1);
    MatrixXd compSq = ((-sqData).rowwise()+sqSum.transpose()) / (N-1);

    VectorXd compSims(N);

    if (mt == Metric::MSD){
        compSims = (2 * (compSq.array() - compC.array().square())/ nAtoms).rowwise().sum();
    } else {
        for (int i=0; i<N; ++i){
            VectorXd objSq = data.row(i).array().square();
            MatrixXd compData (2,N);
            compData.row(0) = cSum.array() - data.row(i).array();
            compData.row(1) = sqSum.array() - objSq.array();
            compSims[i] = extendedComparison(compData, N-1, nAtoms, true);
        }
    }

   return compSims;
}

/* O(N) medoid calculation for n-ary objects.
 *
 * Parameters:
 *  - data: A feature array of shape (nSamples, nFeatures)
 *     --> Can also be a vector (N) of complementary similarities for each object. Useful when calculating medoid and outlier so you only calculate compSims once.
 *  - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems.
 *  - mt: The metric to use when calculating distance between n objects in an array. 
 * 
 * Returns: The index of the medoid in the dataset.
 * 
 * Reference: https://github.com/mqcomplab/MDANCE/blob/main/src/mdance/tools/bts.py#L241
*/
Index calculateMedoid(MatrixXd& data, int nAtoms, Metric mt) {
    VectorXd compSims = calculateCompSim(data, nAtoms, mt);
    return calculateMedoid(compSims, nAtoms, mt);
}
Index calculateMedoid(VectorXd& data, int nAtoms, Metric mt) {
    Index maxIdx;
    data.maxCoeff(&maxIdx);
    return maxIdx;
}

/* O(N) outlier calculation for n-ary objects.
 *
 * Parameters:
 *  - data: A feature array of shape (nSamples, nFeatures)
 *     --> Can also be a vector (N) of complementary similarities for each object. Useful when calculating medoid and outlier so you only calculate compSims once.
 *  - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems.
 *  - mt: The metric to use when calculating distance between n objects in an array. 
 * 
 * Returns: The index of the medoid in the dataset.
 * 
 * Reference: https://github.com/mqcomplab/MDANCE/blob/main/src/mdance/tools/bts.py#L271
*/
Index calculateOutlier(MatrixXd& data, int nAtoms, Metric mt) {
    VectorXd compSims = calculateCompSim(data, nAtoms, mt);
    return calculateOutlier(compSims, nAtoms, mt);
}
Index calculateOutlier(VectorXd& data, int nAtoms, Metric mt) {
    Index minIdx;
    data.minCoeff(&minIdx);
    return minIdx;
}

/* O(N * log(nTrimmed)) method of trimming a desired percentage of outliers (most dissimilar) from a feature array.
 *
 * Parameters:
 *  - data: A feature array of shape (nSamples, nFeatures).
 *  - nTrimmed: The desired # of outliers to be removed. Can be a number (int), or a percentage (float).
 *  - nAtoms: Number of atoms in the Molecular Dynamics (MD) system. nAtoms=1 for non-MD systems.
 *  - isMedoid: Criterion to use for data trimming
 *     --> if (!isMedoid): remove most dissimilar objects based on complement similarity.
 *     --> if (isMedoid): remove most dissimilar objects based on similarity to the medoid.
 * 
 *  Returns: A feature array with the desired outliers removed.
 * 
 *  Reference: https://github.com/mqcomplab/MDANCE/blob/main/src/mdance/tools/bts.py#L301
*/
MatrixXd trimOutliers(MatrixXd& data, int nTrimmed, int nAtoms = 1, bool isMedoid = false, Metric mt) {
    Index N = data.rows();
    VectorXd cSum;
    VectorXd sqSumTotal;
    Index idx;
    if (isMedoid) {
        idx = calculateMedoid(data, nAtoms, mt);
        cSum = data.row(idx);
    } else {
        cSum = data.colwise().sum();
        sqSumTotal = data.array().square().colwise().sum();
    }


    // We will find our outliers by performing a heapSort using a maxHeap of size nTrimmed
    vector<pair<double, int>> compSims;
    compSims.reserve(nTrimmed);
    for (Index i=0; i<nTrimmed; ++i) {
        MatrixXd compData (2,data.row(0).size());
        if (isMedoid) {
            if (i != idx){
                compData.row(0) = data.row(i);
                compData.row(1) = cSum;
                compSims.emplace_back(pair<double,int>(extendedComparison(compData, N-1, nAtoms, false, mt), i));
            } else{
                continue;
            }
        } else {
            compData.row(0) = cSum.array().transpose() - data.row(i).array();
            compData.row(1) = sqSumTotal.array().transpose() - data.row(i).array().square();

            compSims.emplace_back(pair<double,int>(extendedComparison(compData, N-1, nAtoms, true, mt), i));
        }
    }
    std::make_heap(compSims.begin(),compSims.end());
    for (Index i=nTrimmed; i<N; ++i) {
        double simVal;
        MatrixXd compData (2,data.row(0).size());
        if (isMedoid) {
            if (i != idx){
                compData.row(0) = data.row(i);
                compData.row(1) = cSum;
                simVal = extendedComparison(compData, N-1, nAtoms, false, mt);
            } else {
                continue;
            }
        } else {
            compData.row(0) = cSum.array().transpose() - data.row(i).array();
            compData.row(1) = sqSumTotal.array().transpose() - data.row(i).array().square();

            simVal = extendedComparison(compData, N-1, nAtoms, true, mt);
        }

        if (simVal < compSims[0].first) {
            std::pop_heap(compSims.begin(), compSims.end());
            compSims.emplace_back(pair<double,int>(simVal,i));
            std::push_heap(compSims.begin(), compSims.end());
        }
    }
    vector<bool> isInHeap(N, false);
    for (Index i=0; i<nTrimmed; ++i){
        isInHeap[compSims[i].second] = true;
    }
    vector<Index> indices;
    indices.reserve(N - nTrimmed);
    for (Index i=0; i<N; ++i) {
        if(!isInHeap[i]){
            indices.push_back(i);
        }
    }
    return data(indices, Eigen::all);
}
MatrixXd trimOutliers(MatrixXd& data, float nTrimmed, int nAtoms = 1, bool isMedoid = false, Metric mt) {
    return trimOutliers(data, (int) std::floor(data.rows() * nTrimmed), nAtoms, isMedoid, mt);
}
