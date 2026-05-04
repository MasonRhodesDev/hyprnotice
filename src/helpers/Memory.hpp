#pragma once

#include <hyprutils/memory/SharedPtr.hpp>
#include <hyprutils/memory/WeakPtr.hpp>
#include <hyprutils/memory/UniquePtr.hpp>

template <typename T> using SP = Hyprutils::Memory::CSharedPointer<T>;
template <typename T> using WP = Hyprutils::Memory::CWeakPointer<T>;
template <typename T> using UP = Hyprutils::Memory::CUniquePointer<T>;

using Hyprutils::Memory::makeShared;
using Hyprutils::Memory::makeUnique;
