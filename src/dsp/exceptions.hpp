// Exceptions that can be thrown by the application.
// Copyright 2020 Christian Kauten
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef DSP_EXCEPTIONS_HPP_
#define DSP_EXCEPTIONS_HPP_

#include <stdexcept>
#include <string>

/// A DSP error independent of the Rack host.
class DSPException : public std::runtime_error {
 public:
    explicit DSPException(const std::string& message) : std::runtime_error(message) {}
};

/// An exception for trying to set a channel that is out of bounds.
class ChannelOutOfBoundsException: public DSPException {
 public:
    /// @brief Constructor.
    ///
    /// @param index the channel index that was requested
    /// @param count the number of channels that are available
    ///
    ChannelOutOfBoundsException(unsigned index, unsigned count) : DSPException(
        "tried to set output for channel index " +
        std::to_string(index) +
        ", but the chip has " +
        std::to_string(count) +
        " channels"
    ) { }
};


/// An exception for trying to set an address that is out of bounds.
template<typename Address>
class AddressSpaceException: public DSPException {
 public:
    /// @brief Constructor.
    ///
    /// @param at the requested address from the address space
    /// @param start the first address in the address space
    /// @param stop the last address in the address space
    ///
    AddressSpaceException(Address at, Address start, Address stop) : DSPException(
        "tried to access address " +
        std::to_string(at) +
        ", but the chip has address space [" +
        std::to_string(start) +
        ", " +
        std::to_string(stop) +
        "]"
    ) { }
};

#endif  // DSP_EXCEPTIONS_HPP_
