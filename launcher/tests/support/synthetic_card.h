#ifndef LOM_LAUNCHER_SYNTHETIC_CARD_H
#define LOM_LAUNCHER_SYNTHETIC_CARD_H

#include <cstdint>
#include <string>
#include <vector>

namespace lom::launcher::test {
class SyntheticCard {
  public:
    /**
     * @brief Start a formatted empty card.
     */
    SyntheticCard();

    /**
     * @brief Build a valid synthetic Legend of Mana save: card header, checksum and magic.
     * @param seed Content seed.
     * @return 16 KiB of save bytes.
     */
    static std::vector<std::uint8_t> save(std::uint32_t seed);

    /**
     * @brief Place a file across the given blocks, in chain order.
     * @param name Card file name.
     * @param blocks Block numbers from 0 to 14.
     * @param data File bytes, blocks.size() * 8 KiB.
     */
    void add(const std::string& name, const std::vector<int>& blocks, const std::vector<std::uint8_t>& data);

    /**
     * @brief Access the directory frame of a data block.
     * @param block Block number from 0 to 14.
     * @return Frame bytes.
     */
    std::uint8_t* frame(int block);

    /**
     * @brief Recompute a frame's checksum.
     * @param index Frame index, 0 for the header.
     */
    void seal(int index);

    /**
     * @brief Report the image.
     * @return Card bytes.
     */
    const std::vector<std::uint8_t>& bytes() const;

  private:
    std::vector<std::uint8_t> bytes_;
};

}

#endif
