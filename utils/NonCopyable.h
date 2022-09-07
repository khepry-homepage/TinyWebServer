#ifndef NON_COPYABLE_H
#define NON_COPYABLE_H
/**
 * 继承此类可禁用赋值操作，确保资源所有权具有唯一性.
 */
class NonCopyable {
 protected:
  NonCopyable() = default;
  ~NonCopyable() = default;

 private:
  NonCopyable(const NonCopyable &) = delete;
  const NonCopyable &operator=(const NonCopyable &) = delete;
};

#endif