#include <vector>
#include <boost/format.hpp>
#include <algorithm>
#include <fstream>
#include <string>
#include <random>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random.hpp>

using boost::multiprecision::cpp_int;
template <typename Rng, size_t BitCount>
using bit_generator = boost::random::independent_bits_engine<Rng, BitCount, cpp_int>;

template<typename E>
E gcd(E a, E b) {
    E zero(0);
    while (b != zero) {
        a = a % b;
        std::swap(a, b);
    }
    return a;
}

template<typename T>
std::pair<T, T>
quotient_remainder(T a, T b) {
  return { a / b, a % b };
}

template <typename E>
std::pair<E, E> extended_gcd(E a, E b) {
    E x0(1);
    E x1(0);
    while (b != E(0)) {
        // compute new r and x
        std::pair<E, E> qr = quotient_remainder(a, b);
        E x2 = x0 - qr.first * x1;
        // shift r and x
        x0 = x1;
        x1 = x2;
        a = b;
        b = qr.second;
    }
    return {x0, a};
}

template<typename Integer>
constexpr
inline
bool odd(Integer x) {
  return (x & 1) == 1;
}


template<typename Integer>
constexpr
inline
bool even(Integer x) {
  return !odd(x);
}

template<typename Integer>
constexpr
inline
Integer half(Integer x) {
  // precondition(x >= 0)
  return x >> 1;
}

template <typename A, typename N, typename Op>
// requires (Domain<Op, A>)
A power_accumulate_semigroup(A r, A a, N n, Op op) {
    // precondition(n >= 0);
    if (n == 0) return r;
    while (true) {
        if (odd(n)) {
            r = op(r, a);
            if (n == 1) return r;
        }
        n = half(n);
        a = op(a, a);
    }
}


template <typename MultiplicativeSemigroup, typename Integer>
MultiplicativeSemigroup power_accumulate_semigroup(MultiplicativeSemigroup r, MultiplicativeSemigroup a, Integer n) {
    // precondition(n >= 0);
    if (n == 0) return r;
    while (true) {
        if (odd(n)) {
            r = r * a;
            if (n == 1) return r;
        }
        n = half(n);
        a = a * a;
    }
}

template <typename A, typename N, typename Op>
// requires (Domain<Op, A>)
A power_semigroup(A a, N n, Op op) {
    // precondition(n > 0);
    while (!odd(n)) {
        a = op(a, a);
        n = half(n);
    }
    if (n == 1) return a;
    // Auch
    return power_accumulate_semigroup(a, op(a, a), half(N{n - 1}), op);
}


template <typename MultiplicativeSemigroup, typename Integer>
MultiplicativeSemigroup power_semigroup(MultiplicativeSemigroup a, Integer n) {
    // precondition(n > 0);
    while (!odd(n)) {
        a = a * a;
        n = half(n);
    }
    if (n == 1) return a;
    return power_accumulate_semigroup(a, a * a, half(n - 1));
}


template<typename T>
struct modulo_multiply {
  T value;
  modulo_multiply() = default;
  modulo_multiply(const modulo_multiply &) = default;
  modulo_multiply(const T x) : value(x) {}
  T operator()(T x, T y) {
    return (x * y) % value;
  }
};

template<typename T>
std::pair<T, T> factorize(T x, T val) {
  // Bad code, can be better
  T k;
  for (k = 0; x % val == 0; x /= val, ++k);
  return std::make_pair(x, k);
}

template <typename I>
bool miller_rabin_test(I n, I q, I k, I w) {
    // precondition n > 1 && n - 1 = 2^kq && q is odd
    
    modulo_multiply<I> mmult(n);
    I x = power_semigroup(w, q, mmult);
    if (x == I(1) || x == n - I(1)) return true;
    for (I i(1); i < k; ++i) {
    
        // invariant x = w^{2^{i-1}q}
        
        x = mmult(x, x);
        if (x == n - I(1)) return true;
        if (x == I(1))     return false;
    }
    return false;
}


template <typename N>
N stein_gcd(N m, N n) {
    if (m < N(0)) m = -m;
    if (n < N(0)) n = -n;
    if (m == N(0)) return n;
    if (n == N(0)) return m;

    // m > 0 && n > 0

    int d_m = 0;
    while (even(m)) { m >>= 1; ++d_m;}

    int d_n = 0;
    while (even(n)) { n >>= 1; ++d_n;}

    // odd(m) && odd(n)

    while (m != n) {
      if (n > m) std::swap(n, m);
      m -= n;
      do m >>= 1; while (even(m));
    }

    // m == n

    return m << std::min(d_m, d_n);
}

template<typename Integer, typename RandomGenerator>
inline
Integer random_coprime(Integer x, RandomGenerator& rand) {
  Integer number = rand();
  while (number == 0) number = rand();

  Integer gcd_value = gcd(x, number);
  if (gcd_value == Integer{1}) {
    return number;
  }
  ++number;
  return number;
}

template<typename Integer, typename ProbeGenerator>
bool check_primarity(Integer n, ProbeGenerator& rand) {
  if (even(n)) { return false; }
  
  std::pair<Integer, Integer> qk = factorize(Integer{n - 1}, Integer{2});
  const constexpr size_t witnesses = 100;

  for (size_t i = 0; i < witnesses; ++i) {
    Integer w = random_coprime(n, rand);
    if (!miller_rabin_test(n, qk.first, qk.second, w)) {
      return false;
    }
  }
  return true;
}

template<typename PrimarityTest, typename ProbeGenerator, typename WitnessGenerator>
auto probable_prime(PrimarityTest primarity_test, ProbeGenerator& rand, WitnessGenerator& rand_wintness) {
  auto x = rand();
  while (!primarity_test(x, rand_wintness))  {
    x = rand();
  }
  return x;
}

template<typename ProbeGenerator, typename WitnessGenerator>
auto probable_prime(ProbeGenerator& rng, WitnessGenerator& wng) {
  return probable_prime([](const auto &x, auto& rnd) { return check_primarity(x, rnd); }, rng, wng);
}

template <typename I>
I multiplicative_inverse(I a, I n) {
    std::pair<I, I> p = extended_gcd(a, n);
    if (p.second != I(1)) return I(0);
    if (p.first < I(0)) return p.first + n;
    return p.first;
}

template<typename T0, typename T1, typename T2>
struct triple {
  T0 m0;
  T1 m1;
  T2 m2;
  triple(const T0 &m0, const T1 &m1, const T2 &m2) : m0(m0), m1(m1), m2(m2) {}
};

struct rsa_keychain {
  cpp_int n = 0;
  cpp_int public_key = 0;
  cpp_int private_key = 0;
  rsa_keychain(const cpp_int &n, const cpp_int &public_key, const cpp_int &private_key) : n(n),
                                                                                          public_key(public_key),
                                                                                          private_key(private_key) {}
  size_t block_power_of_2() const
  {
    cpp_int value;
    if (public_key == 0) { value = private_key; }
    else if (private_key == 0) { value = public_key; }
    else { value = std::min(public_key, private_key); }
    const cpp_int m = cpp_int{std::min(value, n)};
    return static_cast<size_t>(factorize(m, cpp_int{2}).first);
  }
};

template <typename ProbeGenerator, typename WitnessGenerator>
rsa_keychain rsa_key_generation(ProbeGenerator& p_generator, WitnessGenerator& w_generator) {
  typedef cpp_int Integer;
  while(true) {
  Integer p1 = probable_prime(p_generator, w_generator);
  Integer p2 = probable_prime(p_generator, w_generator);
  Integer n = p1 * p2;
  Integer phi = (p1 - 1) * (p2 - 1);
  Integer public_key = random_coprime(phi, p_generator);
  while (public_key >= phi) {
    public_key = random_coprime(phi, p_generator);
  }
  Integer private_key = multiplicative_inverse(public_key, phi);
  if (private_key != 0)
  return rsa_keychain(n, public_key, private_key);
  }
}

// implement using operator<<
std::pair<std::string, std::string> save(const rsa_keychain &rsa_data, const std::string &file_name) {
  auto first = file_name + ".public";
  std::ofstream public_file(first);
  public_file << std::hex << rsa_data.n << std::endl << rsa_data.public_key;
  auto second = file_name + ".public";
  std::ofstream private_file(second);
  private_file << std::hex << rsa_data.n << std::endl << rsa_data.private_key;
  return {first, second};
}


/*
template<typename OutputIterator>
OutputIterator encrypt(uint8_t *first, uint8_t *last, const cpp_int &n, const cpp_int &public_key, OutputIterator out) {
  constexpr size_t block_size = factorize(n, cpp_int{2});
  auto qr = quotient_remainder(size_t{last - first}, block_size);
  for (size_t i = 0; i != qr.first; ++i) {
    cpp_int val = *(uint64_t*)first;
  }
  return out;
}
*/

cpp_int crypt(cpp_int val, cpp_int n, cpp_int key) {
  return power_semigroup(val, key, modulo_multiply<cpp_int>(n));
}

int main() {
  //boost::mt19937 rng(42);
  bit_generator<std::random_device, 512> probe_generator, witness_generator;
  auto rsa_triple = rsa_key_generation(probe_generator, witness_generator);
  cpp_int value = 65;
  auto encrypted = crypt(value, rsa_triple.n, rsa_triple.public_key);
  auto decrypted = crypt(encrypted, rsa_triple.n, rsa_triple.private_key);
  std::cout << decrypted << std::endl;

}
