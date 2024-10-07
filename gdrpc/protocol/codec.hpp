#include <string>
class Codec {
 public:
  virtual std::string serialize(const std::vector<int>& data) const = 0;

  virtual std::vector<int> deserialize(const std::string& str) const = 0;

  virtual ~Codec() = default;
};