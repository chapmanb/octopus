//
//  vcf_reader.hpp
//  Octopus
//
//  Created by Daniel Cooke on 28/07/2015.
//  Copyright (c) 2015 Oxford University. All rights reserved.
//

#ifndef __Octopus__vcf_reader__
#define __Octopus__vcf_reader__

#include <vector>
#include <cstddef>
#include <memory>
#include <boost/filesystem.hpp>

#include "i_vcf_reader_impl.hpp"

class VcfHeader;
class VcfRecord;
class GenomicRegion;

namespace fs = boost::filesystem;

class VcfReader
{
public:
    // Extracting the sample data from a VCF/BCF file can be very expensive. If this data is not
    // required, performance can be vastly improved by simply not extracting it from file.
    enum class Unpack { All, AllButSamples };
    
    VcfReader()  = delete;
    explicit VcfReader(const fs::path& file_path);
    ~VcfReader() = default;
    
    VcfReader(const VcfReader&)            = default;
    VcfReader& operator=(const VcfReader&) = default;
    VcfReader(VcfReader&&)                 = default;
    VcfReader& operator=(VcfReader&&)      = default;
    
    const fs::path path() const;
    VcfHeader fetch_header() const;
    size_t count_records() const;
    size_t count_records(const GenomicRegion& region) const;
    std::vector<VcfRecord> fetch_records(Unpack level = Unpack::All); // fetches all records
    std::vector<VcfRecord> fetch_records(const GenomicRegion& region, Unpack level = Unpack::All);
    
private:
    fs::path file_path_;
    std::unique_ptr<IVcfReaderImpl> reader_;
};

#endif /* defined(__Octopus__vcf_reader__) */
