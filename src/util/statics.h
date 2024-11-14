// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Static objects with destruction before main() exit.
 */
#ifndef INKSCAPE_UTIL_STATICS_BIN_H
#define INKSCAPE_UTIL_STATICS_BIN_H

#include <optional>

namespace Inkscape::Util {

class StaticHolderBase;

/**
 * The following system provides a way of dealing with statics/singletons with unusual lifetime requirements,
 * specifically the requirement that they be destroyed before the end of main().
 *
 * This isn't guaranteed by the usual static initialisation idiom
 *
 *     X &get()
 *     {
 *         static X x;
 *         return x;
 *     }
 *
 * because X will be destroyed just *after* main() exits. And sometimes that's a deal-breaker!
 *
 *  - To use the system with a singleton class X, derive it from EnableSingleton<X>:
 *
 *        class X : public EnableSingleton<X> { ...
 *
 *    This endows it with a ::get() method that initialises and returns the static instance.
 *
 *    Warning: ::get() is not safe against concurrent initialisation, unlike the idiom above.
 *    So only use it in single-threaded code.
 *
 *  - To ensure that X is outlived by another singleton Y, pass in the dependency using Depends:
 *
 *        class X : public EnableSingleton<X, Depends<Y>> { ...
 *
 *    Multiple dependencies can be specified. Then Y will be destructed after X.
 *
 *    Note: Y will still be lazily-initialised, for startup efficiency. So X's lifetime isn't
 *    necessarily completely contained in Y's lifetime.
 *
 *    Note: As with the above idiom, dependency loops are detected at runtime on glibc.
 *
 *  - To destruct all singletons at any time, call
 *
 *        StaticsBin::get().destroy();
 *
 *    They will be recreated again if re-accessed. This function should be called at the end of main().
 *    If it isn't, it will be detected at runtime by an assertion in StaticsBin::~StaticsBin().
 */

/**
 * Maintains the list of statics that need to be destroyed,
 * destroys them, and complains if it's not asked to do so in time.
 */
class StaticsBin
{
public:
    static StaticsBin &get();

    void destroy();

private:
    ~StaticsBin();

    StaticHolderBase *head = nullptr;

    friend class StaticHolderBase;
};

class StaticHolderBase
{
public:
    StaticHolderBase(StaticHolderBase const &) = delete;
    StaticHolderBase &operator=(StaticHolderBase const &) = delete;

protected:
    StaticHolderBase();

    virtual void destroy() = 0;
    virtual bool active() const = 0;

private:
    StaticHolderBase *const next;

    StaticHolderBase(StaticsBin &bin);

    friend StaticsBin;
};

template <typename... Ts>
struct Depends;

template <typename Deps>
struct DependencyRegisterer {};

template <typename T, typename... Ts>
struct DependencyRegisterer<Depends<T, Ts...>> : DependencyRegisterer<Depends<Ts...>>
{
    DependencyRegisterer()
    {
        T::getStaticHolder();
    }
};

template <typename T, typename Deps = Depends<>>
class StaticHolder
    : private DependencyRegisterer<Deps>
    , private StaticHolderBase
{
public:
    template <typename... Args>
    T &get(Args&&... args)
    {
        [[unlikely]] if (!opt) {
            opt.emplace(std::forward<Args>(args)...);
        }
        return *opt;
    }

protected:
    void destroy() override
    {
        opt.reset();
    }

    bool active() const override
    {
        return opt.has_value();
    }

private:
    struct ConstructibleT : T
    {
        using T::T;
    };

    std::optional<ConstructibleT> opt;
};

template <typename T, typename Deps = Depends<>>
class EnableSingleton
{
public:
    EnableSingleton(EnableSingleton const &) = delete;
    EnableSingleton &operator=(EnableSingleton const &) = delete;

    template <typename... Args>
    static T &get(Args&&... args)
    {
        return getStaticHolder().get(std::forward<Args>(args)...);
    }

    static StaticHolder<T, Deps> &getStaticHolder()
    {
        static StaticHolder<T, Deps> instance;
        return instance;
    }

protected:
    EnableSingleton() = default;
};

} // namespace Inkscape::Util

#endif // INKSCAPE_UTIL_STATICS_BIN_H
