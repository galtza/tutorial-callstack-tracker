#include <string>
#include <random>

namespace qcstudio::misc {

    class uuid4 {
    public:
        uuid4();

        auto str() const -> std::string;
        auto operator<(const uuid4& _other) const -> bool;  // required for containers

    private:
        static auto get_seed() -> uint64_t;

        uint64_t                        high_, low_;
        inline thread_local static auto generator_ = std::independent_bits_engine<std::default_random_engine, 8 * CHAR_BIT, uint64_t>(get_seed());
    };

}  // namespace qcstudio::misc