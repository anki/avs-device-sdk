/*
 * Copyright 2017-2018 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

// WHEN NOT ON LINUX, PORTIONS LICENSED UNDER
/**
 *   Copyright 2011-2015 Quickstep Technologies LLC.
 *   Copyright 2015 Pivotal Software, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 **/

#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <time.h>

#include "AVSCommon/Utils/Timing/TimeUtils.h"
#include "AVSCommon/Utils/Logger/Logger.h"
#include "AVSCommon/Utils/String/StringUtils.h"

#ifndef __linux__
  #define USE_CUSTOM_TIMEGM 1
#else 
  #define USE_CUSTOM_TIMEGM 0
#endif

namespace alexaClientSDK {
namespace avsCommon {
namespace utils {
namespace timing {

using namespace avsCommon::utils::logger;
using namespace avsCommon::utils::string;

/// String to identify log entries originating from this file.
static const std::string TAG("TimeUtils");

/**
 * Create a LogEntry using this file's TAG and the specified event string.
 *
 * @param The event string for this @c LogEntry.
 */
#define LX(event) alexaClientSDK::avsCommon::utils::logger::LogEntry(TAG, event)

/// The length of the year element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_YEAR_STRING_LENGTH = 4;
/// The length of the month element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_MONTH_STRING_LENGTH = 2;
/// The length of the day element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_DAY_STRING_LENGTH = 2;
/// The length of the hour element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_HOUR_STRING_LENGTH = 2;
/// The length of the minute element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_MINUTE_STRING_LENGTH = 2;
/// The length of the second element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_SECOND_STRING_LENGTH = 2;
/// The length of the post-fix element in an ISO-8601 formatted string.
static const int ENCODED_TIME_STRING_POSTFIX_STRING_LENGTH = 4;
/// The dash separator used in an ISO-8601 formatted string.
static const std::string ENCODED_TIME_STRING_DASH_SEPARATOR_STRING = "-";
/// The 'T' separator used in an ISO-8601 formatted string.
static const std::string ENCODED_TIME_STRING_T_SEPARATOR_STRING = "T";
/// The colon separator used in an ISO-8601 formatted string.
static const std::string ENCODED_TIME_STRING_COLON_SEPARATOR_STRING = ":";
/// The plus separator used in an ISO-8601 formatted string.
static const std::string ENCODED_TIME_STRING_PLUS_SEPARATOR_STRING = "+";

/// The offset into an ISO-8601 formatted string where the year begins.
static const unsigned long ENCODED_TIME_STRING_YEAR_OFFSET = 0;
/// The offset into an ISO-8601 formatted string where the month begins.
static const unsigned long ENCODED_TIME_STRING_MONTH_OFFSET = ENCODED_TIME_STRING_YEAR_OFFSET +
                                                              ENCODED_TIME_STRING_YEAR_STRING_LENGTH +
                                                              ENCODED_TIME_STRING_DASH_SEPARATOR_STRING.length();
/// The offset into an ISO-8601 formatted string where the day begins.
static const unsigned long ENCODED_TIME_STRING_DAY_OFFSET = ENCODED_TIME_STRING_MONTH_OFFSET +
                                                            ENCODED_TIME_STRING_MONTH_STRING_LENGTH +
                                                            ENCODED_TIME_STRING_DASH_SEPARATOR_STRING.length();
/// The offset into an ISO-8601 formatted string where the hour begins.
static const unsigned long ENCODED_TIME_STRING_HOUR_OFFSET = ENCODED_TIME_STRING_DAY_OFFSET +
                                                             ENCODED_TIME_STRING_DAY_STRING_LENGTH +
                                                             ENCODED_TIME_STRING_T_SEPARATOR_STRING.length();
/// The offset into an ISO-8601 formatted string where the minute begins.
static const unsigned long ENCODED_TIME_STRING_MINUTE_OFFSET = ENCODED_TIME_STRING_HOUR_OFFSET +
                                                               ENCODED_TIME_STRING_HOUR_STRING_LENGTH +
                                                               ENCODED_TIME_STRING_COLON_SEPARATOR_STRING.length();
/// The offset into an ISO-8601 formatted string where the second begins.
static const unsigned long ENCODED_TIME_STRING_SECOND_OFFSET = ENCODED_TIME_STRING_MINUTE_OFFSET +
                                                               ENCODED_TIME_STRING_MINUTE_STRING_LENGTH +
                                                               ENCODED_TIME_STRING_COLON_SEPARATOR_STRING.length();

/// The total expected length of an ISO-8601 formatted string.
static const unsigned long ENCODED_TIME_STRING_EXPECTED_LENGTH =
    ENCODED_TIME_STRING_SECOND_OFFSET + ENCODED_TIME_STRING_SECOND_STRING_LENGTH +
    ENCODED_TIME_STRING_PLUS_SEPARATOR_STRING.length() + ENCODED_TIME_STRING_POSTFIX_STRING_LENGTH;

/**
 * Utility function that wraps localtime conversion to std::time_t.
 *
 * This function also creates a copy of the given timeStruct since mktime can
 * change the object.
 *
 * @param timeStruct Required pointer to timeStruct to be converted to time_t.
 * @param[out] ret Required pointer to object where the result will be saved.
 * @return Whether the conversion was successful.
 */
// UNUSED FOR NOW. SEE COMMENT IN convertToUtcTimeT
// static bool convertToLocalTimeT(const std::tm* timeStruct, std::time_t* ret) {
//     if (timeStruct == nullptr) {
//         return false;
//     }

//     std::tm tmCopy = *timeStruct;
//     *ret = std::mktime(&tmCopy);
//     return *ret >= 0;
// }

TimeUtils::TimeUtils() : m_safeCTimeAccess{SafeCTimeAccess::instance()} {
}

#if USE_CUSTOM_TIMEGM
// BEGIN LICENSED UNDER APACHE FROM PIVOTAL SOFTWARE
namespace {
    constexpr std::time_t kSecondsInMinute = 60;
    constexpr std::time_t kMinutesInHour = 60;
    constexpr std::time_t kHoursInDay = 24;
    constexpr std::time_t kDaysInYear = 365;
    constexpr std::time_t kSecondsInHour = kSecondsInMinute * kMinutesInHour;
    constexpr std::time_t kSecondsInDay = kSecondsInHour * kHoursInDay;
    constexpr std::time_t kSecondsInYear = kDaysInYear * kSecondsInDay;
    constexpr std::time_t kEpochYearBase = 1970;
    constexpr std::time_t kTMYearBase = 1900;
    constexpr std::time_t kDaysUptoMonth[] = {
        0,
        31,                                                    // Jan
        31 + 28,                                               // Feb
        31 + 28 + 31,                                          // Mar
        31 + 28 + 31 + 30,                                     // Apr
        31 + 28 + 31 + 30 + 31,                                // May
        31 + 28 + 31 + 30 + 31 + 30,                           // Jun
        31 + 28 + 31 + 30 + 31 + 30 + 31,                      // Jul
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,                 // Aug
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,            // Sep
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,       // Oct
        31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,  // Nov
    };
    constexpr std::time_t kDaysUptoMonthLeapYear[] = {
        0,
        31,                                                    // Jan
        31 + 29,                                               // Feb
        31 + 29 + 31,                                          // Mar
        31 + 29 + 31 + 30,                                     // Apr
        31 + 29 + 31 + 30 + 31,                                // May
        31 + 29 + 31 + 30 + 31 + 30,                           // Jun
        31 + 29 + 31 + 30 + 31 + 30 + 31,                      // Jul
        31 + 29 + 31 + 30 + 31 + 30 + 31 + 31,                 // Aug
        31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30,            // Sep
        31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,       // Oct
        31 + 29 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,  // Nov
    };

    inline bool IsLeapYear(std::time_t year) {
      // year is base 1900.
      const bool common_year =
          ((year & 3) != 0) ||
          (((year + kTMYearBase) % 100 == 0) && ((year + kTMYearBase) % 400 != 0));
      return !common_year;
    }

    constexpr std::time_t kPosBase4 = (kEpochYearBase + 3) % 4;
    constexpr std::time_t kPosBase100 = (kEpochYearBase + 99) % 100;
    constexpr std::time_t kPosBase400 = (kEpochYearBase + 399) % 400;
    static_assert(static_cast<std::time_t>(-1) / 2 == 0,
                  "Rounding towards 0 not guaranteed.");
    // Negative bases will be similar as positive, if division rounds to negative
    // infinity.
    constexpr std::time_t kNegBase4 = 4 - (kEpochYearBase % 4);
    constexpr std::time_t kNegBase100 = 100 - (kEpochYearBase % 100);
    constexpr std::time_t kNegBase400 = 400 - (kEpochYearBase % 400);

    // Number of leap days since 1970.
    inline std::time_t NumLeapDays(std::time_t year) {
      // year is base 1970.
      if (year >= 0) {
        return ((year + kPosBase4) / 4) - ((year + kPosBase100) / 100) +
               ((year + kPosBase400) / 400);
      } else {
        return ((year - kNegBase4) / 4) - ((year - kNegBase100) / 100) +
               ((year - kNegBase400) / 400);
      }
    }

}  // namespace

std::time_t timegmCustom(const struct std::tm *tm) {
  const std::time_t years_since_epoch =
      tm->tm_year + kTMYearBase - kEpochYearBase;
  std::time_t time =
      years_since_epoch * kSecondsInYear +
      NumLeapDays(years_since_epoch) * kSecondsInDay;  // Account for leap days.

  // Account for months in current year.
  time += IsLeapYear(tm->tm_year)
              ? kDaysUptoMonthLeapYear[tm->tm_mon] * kSecondsInDay
              : kDaysUptoMonth[tm->tm_mon] * kSecondsInDay;
  // Account for days in current month, hours-mins-seconds in current day.
  time += (tm->tm_mday - 1) * kSecondsInDay + (tm->tm_hour * kSecondsInHour) +
          (tm->tm_min * kSecondsInMinute) + tm->tm_sec;
  return time;
}
// END LICENSED UNDER APACHE FROM PIVOTAL SOFTWARE
#endif

bool TimeUtils::convertToUtcTimeT(const std::tm* utcTm, std::time_t* ret) {
    if (ret == nullptr) {
        ACSDK_ERROR(LX("convertToUtcTimeT").m("return variable is null"));
        return false;
    }

    // ORIGINAL METHOD IS ERROR PRONE:
    // std::time_t converted;
    // std::time_t offset;
    // if (!convertToLocalTimeT(utcTm, &converted) || !localtimeOffset(&offset)) {
    //     ACSDK_ERROR(LX("convertToUtcTimeT").m("failed to convert to local time"));
    //     return false;
    // }
    //
    // adjust converted time
    // *ret = converted - offset;

    #if USE_CUSTOM_TIMEGM
        *ret = timegmCustom(utcTm);
    #else 
        std::tm cpy = *utcTm;
        *ret = timegm(&cpy);
    #endif

    return true;
}

bool TimeUtils::convert8601TimeStringToUnix(const std::string& timeString, int64_t* convertedTime) {
    // TODO : Use std::get_time once we only support compilers that implement this function (GCC 5.1+ / Clang 3.3+)

    if (!convertedTime) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("convertedTime parameter was nullptr."));
        return false;
    }

    std::tm timeInfo;

    if (timeString.length() != ENCODED_TIME_STRING_EXPECTED_LENGTH) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").d("unexpected time string length:", timeString.length()));
        return false;
    }

    if (!stringToInt(
            timeString.substr(ENCODED_TIME_STRING_YEAR_OFFSET, ENCODED_TIME_STRING_YEAR_STRING_LENGTH),
            &(timeInfo.tm_year))) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("error parsing year. Input:" + timeString));
        return false;
    }

    if (!stringToInt(
            timeString.substr(ENCODED_TIME_STRING_MONTH_OFFSET, ENCODED_TIME_STRING_MONTH_STRING_LENGTH),
            &(timeInfo.tm_mon))) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("error parsing month. Input:" + timeString));
        return false;
    }

    if (!stringToInt(
            timeString.substr(ENCODED_TIME_STRING_DAY_OFFSET, ENCODED_TIME_STRING_DAY_STRING_LENGTH),
            &(timeInfo.tm_mday))) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("error parsing day. Input:" + timeString));
        return false;
    }

    if (!stringToInt(
            timeString.substr(ENCODED_TIME_STRING_HOUR_OFFSET, ENCODED_TIME_STRING_HOUR_STRING_LENGTH),
            &(timeInfo.tm_hour))) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("error parsing hour. Input:" + timeString));
        return false;
    }

    if (!stringToInt(
            timeString.substr(ENCODED_TIME_STRING_MINUTE_OFFSET, ENCODED_TIME_STRING_MINUTE_STRING_LENGTH),
            &(timeInfo.tm_min))) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("error parsing minute. Input:" + timeString));
        return false;
    }

    if (!stringToInt(
            timeString.substr(ENCODED_TIME_STRING_SECOND_OFFSET, ENCODED_TIME_STRING_SECOND_STRING_LENGTH),
            &(timeInfo.tm_sec))) {
        ACSDK_ERROR(LX("convert8601TimeStringToUnixFailed").m("error parsing second. Input:" + timeString));
        return false;
    }

    // adjust for C struct tm standard
    timeInfo.tm_isdst = 0;
    timeInfo.tm_year -= 1900;
    timeInfo.tm_mon -= 1;

    std::time_t convertedTimeT;
    bool ok = convertToUtcTimeT(&timeInfo, &convertedTimeT);

    if (!ok) {
        return false;
    }

    *convertedTime = static_cast<int64_t>(convertedTimeT);
    return true;
}

bool TimeUtils::getCurrentUnixTime(int64_t* currentTime) {
    if (!currentTime) {
        ACSDK_ERROR(LX("getCurrentUnixTimeFailed").m("currentTime parameter was nullptr."));
        return false;
    }

    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    *currentTime = static_cast<int64_t>(now);

    return now >= 0;
}

bool TimeUtils::convertTimeToUtcIso8601Rfc3339(
    const std::chrono::high_resolution_clock::time_point& tp,
    std::string* iso8601TimeString) {
    // The length of the RFC 3339 string for the time is maximum 28 characters, include an extra byte for the '\0'
    // terminator.
    char buf[29];
    memset(buf, 0, sizeof(buf));

    // Need to assign it to time_t since time_t in some platforms is long long
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    auto sec = std::chrono::duration_cast<std::chrono::seconds>(ms);
    const time_t timeSecs = static_cast<time_t>(sec.count());

    std::tm utcTm;
    if (!m_safeCTimeAccess->getGmtime(timeSecs, &utcTm)) {
        ACSDK_ERROR(LX("convertTimeToUtcIso8601Rfc3339").m("cannot retrieve tm struct"));
        return false;
    }

    // it's possible for std::strftime to correctly return length = 0, but not with the format string used.  In this
    // case length == 0 is an error.
    auto strftimeResult = std::strftime(buf, sizeof(buf) - 1, "%Y-%m-%dT%H:%M:%S", &utcTm);
    if (strftimeResult == 0) {
        ACSDK_ERROR(LX("convertTimeToUtcIso8601Rfc3339Failed").m("strftime(..) failed"));
        return false;
    }

    std::stringstream millisecondTrailer;
    millisecondTrailer << buf << "." << std::setfill('0') << std::setw(3) << (ms.count() % 1000) << "Z";

    *iso8601TimeString = millisecondTrailer.str();
    return true;
}

// ERROR PRONE:
// bool TimeUtils::localtimeOffset(std::time_t* ret) {
//     static const std::chrono::time_point<std::chrono::system_clock> timePoint{std::chrono::hours(24)};
//     auto fixedTime = std::chrono::system_clock::to_time_t(timePoint);

//     std::tm utcTm;
//     std::time_t utc;
//     std::tm localTm;
//     std::time_t local;
//     if (!m_safeCTimeAccess->getGmtime(fixedTime, &utcTm) || !convertToLocalTimeT(&utcTm, &utc) ||
//         !m_safeCTimeAccess->getLocaltime(fixedTime, &localTm) || !convertToLocalTimeT(&localTm, &local)) {
//         ACSDK_ERROR(LX("localtimeOffset").m("cannot retrieve tm struct"));
//         return false;
//     }

//     *ret = utc - local;
//     return true;
// }

}  // namespace timing
}  // namespace utils
}  // namespace avsCommon
}  // namespace alexaClientSDK
