// File: cpp/tools/markdown_table_renderer.hpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct MarkdownTableColumn {
    std::string header;
    bool right_aligned{};
};

struct MarkdownTableRow {
    std::vector<std::string> cells;
};

std::vector<std::size_t> DetermineMarkdownColumnWidths(
    const std::vector<MarkdownTableColumn>& columns,
    const std::vector<MarkdownTableRow>& rows);

std::string FormatMarkdownTable(
    const std::vector<MarkdownTableColumn>& columns,
    const std::vector<MarkdownTableRow>& rows);

bool IsMarkdownTableSeparatorLine(
    const std::string& line,
    std::size_t expected_column_count);

std::size_t CountMarkdownTableRows(const std::string& markdown);

bool MarkdownTableRendererSelfTest();
