#ifndef LEO_NTN_HANDOVER_OUTPUT_LIFECYCLE_H
#define LEO_NTN_HANDOVER_OUTPUT_LIFECYCLE_H

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace ns3
{

namespace HandoverCsvHeaders
{
inline constexpr std::string_view kHexGridCatalog =
    "id,latitude_deg,longitude_deg,altitude_m,east_m,north_m,ecef_x,ecef_y,ecef_z\n";
inline constexpr std::string_view kSatAnchorTrace =
    "time_s,sat,plane,slot,cell,anchor_grid_id,anchor_latitude_deg,anchor_longitude_deg,"
    "anchor_east_m,anchor_north_m\n";
inline constexpr std::string_view kSatGroundTrackTrace =
    "time_s,sat,plane,slot,cell,subpoint_latitude_deg,subpoint_longitude_deg,"
    "subpoint_east_m,subpoint_north_m,sat_ecef_x,sat_ecef_y,sat_ecef_z\n";
inline constexpr std::string_view kHandoverEventTrace =
    "time_s,ue,ho_id,event,source_cell,target_cell,source_sat,target_sat,delay_ms,"
    "ping_pong_detected,failure_reason\n";
inline constexpr std::string_view kPhyDlTbTrace =
    "time_s,ue,cell,serving_sat,rnti,bwp,frame,subframe,slot,sym_start,num_sym,"
    "tb_size,mcs,rank,rv,rb_assigned,cqi,sinr_db,min_sinr_db,tbler,corrupt\n";
inline constexpr std::string_view kHandoverThroughputTrace =
    "time_s,ue,serving_cell,serving_sat,throughput_mbps,delta_rx_packets,"
    "total_rx_packets,in_handover,active_ho_id,pending_source_cell,pending_target_cell\n";
inline constexpr std::string_view kE2eFlowMetrics =
    "ue,dl_port,matched_flow,tx_packets,rx_packets,lost_packets,loss_rate_percent,tx_bytes,"
    "rx_bytes,offered_mbps,throughput_mbps,mean_delay_ms,mean_jitter_ms\n";
inline constexpr std::string_view kPhyDlTbMetrics =
    "ue,tb_count,corrupt_tb_count,corrupt_tb_rate_percent,mean_tbler,mean_sinr_db,min_sinr_db\n";
inline constexpr std::string_view kPhyDlTbIntervalMetrics =
    "interval_index,window_start_s,window_end_s,tb_count,corrupt_tb_count,"
    "corrupt_tb_rate_percent,mean_tbler,mean_sinr_db,min_sinr_db\n";
} // namespace HandoverCsvHeaders

inline bool
EnsureOutputParentDirectoryForFile(const std::string& filePath)
{
    const std::filesystem::path path(filePath);
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty())
    {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    return !ec;
}

template <typename... Paths>
inline bool
EnsureParentDirectoriesForFiles(const Paths&... filePaths)
{
    return (EnsureOutputParentDirectoryForFile(filePaths) && ...);
}

inline void
CloseOutputStream(std::ofstream& stream)
{
    if (stream.is_open())
    {
        stream.close();
    }
    stream.clear();
}

template <typename... Streams>
inline void
CloseOutputStreams(Streams&... streams)
{
    (CloseOutputStream(streams), ...);
}

inline bool
ResetCsvOutputStream(std::ofstream& stream,
                     const std::string& path,
                     std::string_view header = std::string_view())
{
    CloseOutputStream(stream);
    stream.open(path, std::ios::out | std::ios::trunc);
    if (!stream.is_open())
    {
        return false;
    }
    if (!header.empty())
    {
        stream << header;
    }
    return stream.good();
}

template <typename Writer>
inline bool
WriteCsvFile(const std::string& path, std::string_view header, Writer writer)
{
    std::ofstream out;
    if (!ResetCsvOutputStream(out, path, header))
    {
        return false;
    }
    writer(out);
    out.flush();
    return out.good();
}

} // namespace ns3

#endif // LEO_NTN_HANDOVER_OUTPUT_LIFECYCLE_H
