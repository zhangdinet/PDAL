/******************************************************************************
* Copyright (c) 2011, Michael P. Gerlek (mpg@flaxen.com)
*
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following
* conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in
*       the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Hobu, Inc. or Flaxen Geo Consulting nor the
*       names of its contributors may be used to endorse or promote
*       products derived from this software without specific prior
*       written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
* FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
* COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
* OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
* AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
* OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
* OF SUCH DAMAGE.
****************************************************************************/

#include <pdal/filters/Stats.hpp>

#include <pdal/PointBuffer.hpp>

namespace pdal { namespace filters {

//---------------------------------------------------------------------------

StatsCollector::StatsCollector()
    : m_count(0)
    , m_minimum(0.0)
    , m_maximum(0.0)
    , m_sum(0.0)
{
    return;
}


void StatsCollector::reset()
{
    m_count = 0;
    m_minimum = 0.0;
    m_maximum = 0.0;
    m_sum = 0.0;
    return;
}


void StatsCollector::insert(double value)
{
    if (m_count==0)
    {
        m_minimum = value;
        m_maximum = value;
        m_sum = value;
    }
    else
    {
        m_minimum = std::min(m_minimum, value);
        m_maximum = std::max(m_maximum, value);
        m_sum += value;
    }

    ++m_count;

    return;
}


boost::property_tree::ptree StatsCollector::toPTree() const
{
    boost::property_tree::ptree tree;

    tree.put("count", count());
    tree.put("minimum", minimum());
    tree.put("maximum", maximum());
    tree.put("average", average());

    return tree;
}


//---------------------------------------------------------------------------


Stats::Stats(Stage& prevStage, const Options& options)
    : pdal::Filter(prevStage, options)
{
    return;
}


Stats::Stats(Stage& prevStage)
    : Filter(prevStage, Options::none())
{
    return;
}


Stats::~Stats()
{
    const std::vector<Dimension>& dims = getSchema().getDimensions();
    for (std::vector<Dimension>::const_iterator iter = dims.begin(); iter != dims.end(); ++iter)
    {
        const Dimension& dim = *iter;
        StatsCollector* stats = m_stats[dim.getId()];
        delete stats;
        m_stats.erase(dim.getId());
    }
}

void Stats::initialize()
{
    Filter::initialize();

    const std::vector<Dimension>& dims = getSchema().getDimensions();
    for (std::vector<Dimension>::const_iterator iter = dims.begin(); iter != dims.end(); ++iter)
    {
        const Dimension& dim = *iter;
        m_stats[dim.getId()] = new StatsCollector();
    }

    return;
}


const Options Stats::getDefaultOptions() const
{
    Options options;
    return options;
}


void Stats::reset()
{
    const std::vector<Dimension>& dims = getSchema().getDimensions();
    for (std::vector<Dimension>::const_iterator iter = dims.begin(); iter != dims.end(); ++iter)
    {
        const Dimension& dim = *iter;
        m_stats[dim.getId()]->reset();
    }
}
    

const StatsCollector& Stats::getStats(DimensionId::Id field) const
{
    const StatsCollector* s = m_stats.find(field)->second;
    return *s;
}


void Stats::processBuffer(PointBuffer& data) const
{
    const boost::uint32_t numPoints = data.getNumPoints();

    const Schema& schema = data.getSchema();

    // BUG: fix this!
    const int indexXi = schema.getDimensionIndex(DimensionId::X_i32);
    const int indexYi = schema.getDimensionIndex(DimensionId::Y_i32);
    const int indexZi = schema.getDimensionIndex(DimensionId::Z_i32);
    const int indexXd = schema.getDimensionIndex(DimensionId::X_f64);
    const int indexYd = schema.getDimensionIndex(DimensionId::Y_f64);
    const int indexZd = schema.getDimensionIndex(DimensionId::Z_f64);

    StatsCollector& statsX = (indexXi!=-1) ? *(m_stats.find(DimensionId::X_i32)->second) : *(m_stats.find(DimensionId::X_f64)->second);
    StatsCollector& statsY = (indexYi!=-1) ? *(m_stats.find(DimensionId::Y_i32)->second) : *(m_stats.find(DimensionId::Y_f64)->second);
    StatsCollector& statsZ = (indexZi!=-1) ? *(m_stats.find(DimensionId::Z_i32)->second) : *(m_stats.find(DimensionId::Z_f64)->second);

    for (boost::uint32_t pointIndex=0; pointIndex<numPoints; pointIndex++)
    {
        const double x = (indexXi!=-1) ? data.getField<boost::int32_t>(pointIndex, indexXi) : data.getField<double>(pointIndex, indexXd);
        const double y = (indexYi!=-1) ? data.getField<boost::int32_t>(pointIndex, indexYi) : data.getField<double>(pointIndex, indexYd);
        const double z = (indexZi!=-1) ? data.getField<boost::int32_t>(pointIndex, indexZi) : data.getField<double>(pointIndex, indexZd);

        statsX.insert(x);
        statsY.insert(y);
        statsZ.insert(z);

        data.setNumPoints(pointIndex+1);
    }

    return;
}


pdal::StageSequentialIterator* Stats::createSequentialIterator() const
{
    return new pdal::filters::iterators::sequential::Stats(*this);
}


boost::property_tree::ptree Stats::toStatsPTree() const
{
    boost::property_tree::ptree tree;

    for (std::map<DimensionId::Id, StatsCollector*>::const_iterator iter = m_stats.begin();
         iter != m_stats.end();
         ++iter)
    {
        const StatsCollector* stat = iter->second;
        boost::property_tree::ptree subtree = stat->toPTree();

        // BUG: make this work for all fields
        if (iter->first == DimensionId::X_i32 || iter->first == DimensionId::X_f64)
        {
            tree.add_child("X", subtree);
        }
        else if (iter->first == DimensionId::Y_i32 || iter->first == DimensionId::Y_f64)
        {
            tree.add_child("Y", subtree);
        }
        else if (iter->first == DimensionId::Z_i32 || iter->first == DimensionId::Z_f64)
        {
            tree.add_child("Z", subtree);
        }
    }

    return tree;
}


namespace iterators { namespace sequential {


Stats::Stats(const pdal::filters::Stats& filter)
    : pdal::FilterSequentialIterator(filter)
    , m_statsFilter(filter)
{
    return;
}


boost::uint32_t Stats::readBufferImpl(PointBuffer& data)
{
    const boost::uint32_t numRead = getPrevIterator().read(data);

    m_statsFilter.processBuffer(data);

    return numRead;
}


boost::uint64_t Stats::skipImpl(boost::uint64_t count)
{
    getPrevIterator().skip(count);
    return count;
}


bool Stats::atEndImpl() const
{
    return getPrevIterator().atEnd();
}

} } // iterators::sequential

} } // namespaces