/*
 FITS - Flexible Inference from Time-Series data
 (c) 2016-2019 by Tal Zinger
 tal.zinger@outlook.com
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "ResultsStats.hpp"


void ResultsStats::CalculateStatsPopulationSize()
{
    _num_results = _result_vector.size();
    
    boost::accumulators::accumulator_set<
    FLOAT_TYPE,
    boost::accumulators::stats<
    boost::accumulators::tag::variance,
    boost::accumulators::tag::mean,
    boost::accumulators::tag::median,
    boost::accumulators::tag::min,
    boost::accumulators::tag::max> > acc_distance;
    
    boost::accumulators::accumulator_set<
    FLOAT_TYPE,
    boost::accumulators::stats<
    boost::accumulators::tag::variance,
    boost::accumulators::tag::mean,
    boost::accumulators::tag::median,
    boost::accumulators::tag::min,
    boost::accumulators::tag::max> > acc_population_size;
    
    
    std::vector<int> popsize_storage;
    for ( auto sim_result : _result_vector ) {
        acc_distance(sim_result.distance_from_actual);
        
        acc_population_size(sim_result.N);
        
        //std::cout << "N=" << sim_result.N << std::endl;
        popsize_storage.push_back(sim_result.N);
    }
    
    
    _pop_min = boost::accumulators::min(acc_population_size);
    _pop_max = boost::accumulators::max(acc_population_size);
    _pop_mean = boost::accumulators::mean(acc_population_size);
    _pop_sd = std::sqrt(boost::accumulators::variance(acc_population_size));
    
    //_pop_median = GetMedian(popsize_storage);
    _pop_median = boost::accumulators::median(acc_population_size);
    
    boost::accumulators::accumulator_set<
    FLOAT_TYPE,
    boost::accumulators::stats<
    boost::accumulators::tag::median > > acc_distance_for_mad;
    
    
    for ( auto current_N : popsize_storage ) {
        acc_distance_for_mad( std::fabs( current_N - _pop_median ) );
    }
    
    _pop_mad = boost::accumulators::median( acc_distance_for_mad );
      
    _distance_min = boost::accumulators::min(acc_distance);
    _distance_max = boost::accumulators::max(acc_distance);
    _distance_mean = boost::accumulators::mean(acc_distance);
    _distance_sd = std::sqrt(boost::accumulators::variance(acc_distance));
    
    // levene's test for population size
    levenes_pval.resize(1, -1.0f);
    // PriorDistributionType prior_type = PriorDistributionType::UNIFORM;
    
    std::vector<int> minN {_zparams.GetInt(fits_constants::PARAM_MIN_LOG_POPSIZE)};
    std::vector<int> maxN {_zparams.GetInt(fits_constants::PARAM_MAX_LOG_POPSIZE)};
    
    //PriorSampler<int> sampler( minN, maxN, PriorDistributionType::UNIFORM );
    
    //auto popsize_vector_list = sampler.SamplePrior(_num_results);
    //std::vector<int> prior_vec_int;
    //for ( auto current_vec : popsize_vector_list ) {
    //    prior_vec_int.push_back( std::pow( 10, current_vec[0] ) );
    //}
    
    //auto prior_vec_int = sampler.SamplePrior(_num_results)[1];
    
    /*
     std::cout << "prior size=" << prior_vec_int.size() << std::endl;
     for ( auto current_n : prior_vec_int ) {
     std::cout << current_n << "\t";
     }
     std::cout << std::endl;
     
     std::cout << "posteiror size=" << popsize_storage.size() << std::endl;
     for ( auto current_n : popsize_storage ) {
     std::cout << current_n << "\t";
     }
     std::cout << std::endl;
     */
    
    std::vector<FLOAT_TYPE> posterior_vec;
    std::vector<FLOAT_TYPE> prior_vec_float; // this provides conversion
    
    for ( auto current_vec : _prior_distrib ) {
        prior_vec_float.push_back( std::pow( 10, current_vec[0] ) );
    }
    // prior_vec_float.resize( prior_vec_int.size() );
    posterior_vec.resize( prior_vec_float.size() );
    
    //std::copy( prior_vec_int.cbegin(), prior_vec_int.cend(), prior_vec_float.begin() );
    std::copy( popsize_storage.cbegin(), popsize_storage.cend(), posterior_vec.begin() );
    
    /*
     std::transform( prior_vec_int.cbegin(), prior_vec_int.cend(),
     prior_vec_float.begin(),
     []( int val ) {
     return static_cast<float>(val);
     } );
     */
    /*
     std::transform( popsize_storage.cbegin(), popsize_storage.cend(),
     posterior_vec.begin(),
     []( int val ) {
     return static_cast<float>(val);
     } );
     */
    
    /*
     std::cout << "prior2 size=" << prior_vec_float.size() << std::endl;
     for ( auto current_n : prior_vec_float ) {
     std::cout << current_n << "\t";
     }
     std::cout << std::endl;
     
     std::cout << "posteiror2 size=" << posterior_vec.size() << std::endl;
     for ( auto current_n : posterior_vec ) {
     std::cout << current_n << "\t";
     }
     std::cout << std::endl;
     */
    
    levenes_pval[0] = LevenesTest2(posterior_vec, prior_vec_float );
    
    //std::cout << "levenes p = " << levenes_pval[0] <<  std::endl;
}


std::string ResultsStats::GetSummaryPopSize( bool table_only )
{
    std::stringstream ss;
    
    // delimited text meant to be passed to spreadsheet or downstream
    // non-human analysis (no beutification)
    if ( table_only ) {
        
        ss << "median" << fits_constants::FILE_FIELD_DELIMITER;
        ss << "MAD" << fits_constants::FILE_FIELD_DELIMITER;
        ss << "min" << fits_constants::FILE_FIELD_DELIMITER;
        ss << "max" << fits_constants::FILE_FIELD_DELIMITER;
        ss << "pval";
        ss << std::endl;
        
        if ( levenes_pval[0] < fits_constants::LEVENES_SIGNIFICANCE ) {
            ss << boost::format("%-.2e") % _pop_median << fits_constants::FILE_FIELD_DELIMITER;
        }
        else {
            // add asterisk
            ss << "*" << boost::format("%-.2e") % _pop_median << fits_constants::FILE_FIELD_DELIMITER;
        }
        
        ss << boost::format("%-.2e") % _pop_mad << fits_constants::FILE_FIELD_DELIMITER;
        ss << boost::format("%-.2e") % _pop_min << fits_constants::FILE_FIELD_DELIMITER;
        ss << boost::format("%-.2e") % _pop_max << fits_constants::FILE_FIELD_DELIMITER;
        ss << boost::format("%-.2e") % levenes_pval[0];
        
        //ss << _pop_mad << fits_constants::FILE_FIELD_DELIMITER;
        //ss << _pop_min << fits_constants::FILE_FIELD_DELIMITER;
        //ss << _pop_max << fits_constants::FILE_FIELD_DELIMITER;
        //ss << levenes_pval[0];
        
        return ss.str();
    }
    
    
    /************************/
    // on-screen report
    
    
    ss << "Population Size Report" << std::endl;
    ss << GetSummaryHeader();
    
    if ( _single_mutrate_used ) {
        ss << "Used a single mutation rate." << std::endl;
    }
    
    auto tmp_scaling_str = _zparams.GetString( fits_constants::PARAM_SCALING,
                                              fits_constants::PARAM_SCALING_DEFAULT );
    
    if ( tmp_scaling_str.compare(fits_constants::PARAM_SCALING_OFF) == 0 ) {
        // ss << "Data has not been scaled" << std::endl;
    }
    else {
        ss << "Data was scaled using " << tmp_scaling_str << std::endl;
    }
    
    
    if ( _single_mutrate_used ) {
        ss << "Used a single mutation rate." << std::endl;
    }
    else {
        ss << "Used individual mutation rates." << std::endl;
    }
    
    ss << "Distance metric: " << _distance_metric << std::endl;
    
    ss << "====================" << std::endl;
    
    ss << boost::format("%-12s") % "median";
    ss << boost::format("%-12s") % "MAD";
    //ss << boost::format("%-12s") % "mean";
    ss << boost::format("%-12s") % "min";
    ss << boost::format("%-12s") % "max";
    //ss << boost::format("%-10s") % "minldist";
    //ss << boost::format("%-10s") % "maxldist";
    ss << boost::format("%-12s") % "pval";
    ss << std::endl;
    
    if ( levenes_pval[0] < fits_constants::LEVENES_SIGNIFICANCE ) {
        ss << boost::format("%-12.2e") % _pop_median;
    }
    else {
        // add asterisk
        ss << boost::format("*%-12.2e") % _pop_median;
    }
    
    ss << boost::format("%-12.2e") % _pop_mad;
    
    //ss << boost::format("%-12.2e") % _pop_mean;
    ss << boost::format("%-12.2e") % _pop_min;
    ss << boost::format("%-12.2e") % _pop_max;
    //ss << boost::format("%-10.3d") % _distance_min;
    //ss << boost::format("%-10.3d") % _distance_max;
    ss << boost::format("%-12.2d") % levenes_pval[0];
    ss << std::endl;
    
    
    if ( _zparams.GetInt( fits_constants::PARAM_DUMP_PARAMETERS, 0) > 1 ) {
        ss << _zparams.GetAllParameters() << std::endl;
    }
    
    return ss.str();
}
