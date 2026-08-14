// File: cpp/tools/markdown_table_renderer.cpp
#include "markdown_table_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kMinimumColumnWidth = 3U;

bool IsCellContentValid(const std::string& cell) noexcept {
    return cell.find('|') == std::string::npos &&
           cell.find('\n') == std::string::npos &&
           cell.find('\r') == std::string::npos;
}

bool IsTableDefinitionValid(
    const std::vector<MarkdownTableColumn>& columns,
    const std::vector<MarkdownTableRow>& rows) {
    if (columns.empty()) {
        return false;
    }

    for (const MarkdownTableColumn& column : columns) {
        if (!IsCellContentValid(column.header)) {
            return false;
        }
    }

    for (const MarkdownTableRow& row : rows) {
        if (row.cells.size() != columns.size()) {
            return false;
        }

        for (const std::string& cell : row.cells) {
            if (!IsCellContentValid(cell)) {
                return false;
            }
        }
    }

    return true;
}

std::string RightPad(const std::string& value, const std::size_t width) {
    if (value.size() >= width) {
        return value;
    }
    return value + std::string(width - value.size(), ' ');
}

std::string LeftPad(const std::string& value, const std::size_t width) {
    if (value.size() >= width) {
        return value;
    }
    return std::string(width - value.size(), ' ') + value;
}

std::string BuildSeparatorCell(const std::size_t width,
                               const bool right_aligned) {
    if (right_aligned) {
        return std::string(width - 1U, '-') + ':';
    }
    return ':' + std::string(width - 1U, '-');
}

std::vector<std::string> SplitPipes(const std::string& line) {
    std::vector<std::string> cells;
    if (line.size() < 2U || line.front() != '|' || line.back() != '|') {
        return cells;
    }

    std::size_t start = 1U;
    while (start < line.size()) {
        const std::size_t separator = line.find('|', start);
        if (separator == std::string::npos) {
            return {};
        }
        cells.push_back(line.substr(start, separator - start));
        start = separator + 1U;
    }

    return cells;
}

bool IsSeparatorCell(const std::string& cell) {
    if (cell.size() < kMinimumColumnWidth) {
        return false;
    }

    const bool left_colon = cell.front() == ':';
    const bool right_colon = cell.back() == ':';
    const std::size_t begin = left_colon ? 1U : 0U;
    const std::size_t end = right_colon ? cell.size() - 1U : cell.size();

    if (begin >= end) {
        return false;
    }

    for (std::size_t index = begin; index < end; ++index) {
        if (cell[index] != '-') {
            return false;
        }
    }

    return true;
}

}  // namespace

std::vector<std::size_t> DetermineMarkdownColumnWidths(
    const std::vector<MarkdownTableColumn>& columns,
    const std::vector<MarkdownTableRow>& rows) {
    if (!IsTableDefinitionValid(columns, rows)) {
        return {};
    }

    std::vector<std::size_t> widths(columns.size(), kMinimumColumnWidth);

    for (std::size_t index = 0U; index < columns.size(); ++index) {
        widths[index] = std::max(widths[index], columns[index].header.size());
    }

    for (const MarkdownTableRow& row : rows) {
        for (std::size_t index = 0U; index < row.cells.size(); ++index) {
            widths[index] = std::max(widths[index], row.cells[index].size());
        }
    }

    return widths;
}

std::string FormatMarkdownTable(
    const std::vector<MarkdownTableColumn>& columns,
    const std::vector<MarkdownTableRow>& rows) {
    const std::vector<std::size_t> widths =
        DetermineMarkdownColumnWidths(columns, rows);

    if (widths.empty()) {
        return {};
    }

    std::ostringstream output;
    output << '|';

    for (std::size_t index = 0U; index < columns.size(); ++index) {
        output << RightPad(columns[index].header, widths[index]) << '|';
    }
    output << '\n' << '|';

    for (std::size_t index = 0U; index < columns.size(); ++index) {
        output << BuildSeparatorCell(widths[index], columns[index].right_aligned)
               << '|';
    }

    for (const MarkdownTableRow& row : rows) {
        output << '\n' << '|';
        for (std::size_t index = 0U; index < row.cells.size(); ++index) {
            const std::string formatted = columns[index].right_aligned
                                              ? LeftPad(row.cells[index], widths[index])
                                              : RightPad(row.cells[index], widths[index]);
            output << formatted << '|';
        }
    }

    return output.str();
}

bool IsMarkdownTableSeparatorLine(
    const std::string& line,
    const std::size_t expected_column_count) {
    const std::vector<std::string> cells = SplitPipes(line);
    if (cells.size() != expected_column_count || cells.empty()) {
        return false;
    }

    return std::all_of(
        cells.begin(),
        cells.end(),
        [](const std::string& cell) {
            return IsSeparatorCell(cell);
        });
}

std::size_t CountMarkdownTableRows(const std::string& markdown) {
    std::istringstream input(markdown);
    std::string line;
    std::size_t count = 0U;
    bool header_seen = false;
    bool separator_seen = false;

    while (std::getline(input, line)) {
        const std::vector<std::string> cells = SplitPipes(line);
        if (cells.empty()) {
            continue;
        }

        if (!header_seen) {
            header_seen = true;
            continue;
        }

        if (!separator_seen) {
            if (!IsMarkdownTableSeparatorLine(line, cells.size())) {
                return 0U;
            }
            separator_seen = true;
            continue;
        }

        ++count;
    }

    return separator_seen ? count : 0U;
}

bool MarkdownTableRendererSelfTest() {
    const std::vector<MarkdownTableColumn> columns{
        MarkdownTableColumn{"Metric", false},
        MarkdownTableColumn{"Value", true}};

    const std::vector<MarkdownTableRow> rows{
        MarkdownTableRow{{"Risk", "0.20"}},
        MarkdownTableRow{{"Safe", "true"}}};

    const std::string expected =
        "|Metric|Value|\n"
        "|:-----|---:|\n"
        "|Risk  | 0.20|\n"
        "|Safe  | true|";

    const std::string table = FormatMarkdownTable(columns, rows);
    if (table != expected ||
        CountMarkdownTableRows(table) != 2U ||
        !IsMarkdownTableSeparatorLine("|:-----|---:|", 2U)) {
        return false;
    }

    const std::vector<std::size_t> widths =
        DetermineMarkdownColumnWidths(columns, rows);
    if (widths.size() != 2U || widths[0] != 6U || widths[1] != 5U) {
        return false;
    }

    const std::vector<MarkdownTableRow> invalid_cell{
        MarkdownTableRow{{"bad|cell", "value"}}};
    if (!FormatMarkdownTable(columns, invalid_cell).empty()) {
        return false;
    }

    const std::vector<MarkdownTableRow> wrong_count{
        MarkdownTableRow{{"only_one"}}};
    if (!FormatMarkdownTable(columns, wrong_count).empty()) {
        return false;
    }

    return !IsMarkdownTableSeparatorLine("|:--|---:|", 2U) &&
           CountMarkdownTableRows("|Metric|\n|:-----|\n|Risk|") == 1U;
}
