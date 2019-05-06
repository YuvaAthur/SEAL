// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "examples.h"

using namespace std;
using namespace seal;

void example_basic_bfv()
{
    print_example_banner("Example: Basic BFV");

    /*
    In this example, we demonstrate performing simple computations (a polynomial
    evaluation) on encrypted integers.

    Microsoft SEAL implements two encryption schemes:
        - the Brakerski/Fan-Vercauteren (BFV) scheme and
        - the Cheon-Kim-Kim-Song (CKKS) scheme.
    We use the BFV scheme in this example as it is far easier to understand and
    to use than CKKS. The public interfaces of BFV and CKKS differ very little
    in Microsoft SEAL. For more details on the basics of the BFV scheme, we
    refer the reader to the original paper https://eprint.iacr.org/2012/144.
    To achieve good performance, Microsoft SEAL implements the "FullRNS"
    optimization as described in https://eprint.iacr.org/2016/510. This
    optimization is invisible to the user and has no security implications. We
    will discuss the CKKS scheme in later examples.
    */

    /*
    The first task is to set up an instance of the EncryptionParameters class.
    It is critical to understand how the different parameters behave, how they
    affect the encryption scheme, performance, and the security level. There are
    three encryption parameters that are necessary to set:

        - poly_modulus_degree (degree of polynomial modulus);
        - coeff_modulus ([ciphertext] coefficient modulus);
        - plain_modulus (plaintext modulus, only for the BFV scheme).

    A fourth parameter -- noise_standard_deviation -- has a default value 3.20
    and should not be necessary to modify unless the user has a specific reason
    to do so and has an in-depth understanding of the security implications.

    A fifth parameter -- random_generator -- can be set to use customized random
    number generators. By default, Microsoft SEAL uses hardware-based AES in
    counter mode for pseudo-randomness, with a random key generated using
    std::random_device. If the AES-NI instruction set is not available, all
    randomness is generated from std::random_device. Most users should have
    little reason to change this behavior.

    The BFV scheme cannot perform arbitrary computations on encrypted data.
    Instead, each ciphertext has a specific quantity called the `invariant noise
    budget' -- or `noise budget' for short -- measured in bits. The noise budget
    in a freshly encrypted ciphertext (initial noise budget) is determined by
    the encryption parameters. Homomorphic operations consume the noise budget
    at a rate also determined by the encryption parameters. In BFV the two basic
    operations allowed on encrypted data are additions and multiplications, of
    which additions can generally be thought of as being nearly free in terms of
    noise budget consumption compared to multiplications. Since noise budget
    consumption compounds in sequential multiplications, the most significant
    factor in choosing appropriate encryption parameters is the multiplicative
    depth of the arithmetic circuit that the user wants to evaluate on encrypted
    data. Once the noise budget of a ciphertext reaches zero it becomes too
    corrupted to be decrypted. Thus, it is essential to choose the parameters to
    be large enough to support the desired computation; otherwise the result is
    impossible to make sense of even with the secret key.
    */
    EncryptionParameters parms(scheme_type::BFV);

    /*
    The first parameter we set is the degree of the polynomial modulus. This must
    be a positive power of 2, representing the degree of a power-of-2 cyclotomic
    polynomial; it is not necessary to understand what this means. The polynomial
    modulus degree should be thought of mainly affecting the security level of the
    scheme: larger degree makes the scheme more secure. Larger degree also makes
    ciphertext sizes larger, and consequently all operations slower. Recommended
    degrees are 1024, 2048, 4096, 8192, 16384, 32768, but it is also possible to
    go beyond this range. In this example we use a relatively small polynomial
    modulus.
    */
    parms.set_poly_modulus_degree(4096);

    /*
    Next we set the [ciphertext] coefficient modulus (coeff_modulus). The size
    of the coefficient modulus should be thought of as the most significant
    factor in determining the noise budget in a freshly encrypted ciphertext:
    bigger means more noise budget, which is desirable. On the other hand,
    a larger coefficient modulus lowers the security level of the scheme. Thus,
    if a large noise budget is required for complicated computations, a large
    coefficient modulus needs to be used, and the reduction in the security
    level must be countered by simultaneously increasing the polynomial modulus.
    Overall, this will result in worse performance per operation.

    From the above comments the user should remember that the degree of the
    polynomial modulus along with the coefficient modulus determine how secure
    the encryption scheme is, and that these two parameters control a delicate
    balance of functionality, performance, and security. Estimating the security
    of a specific set of parameters is challenging and requires the help of an
    expert in homomorphic encryption.

    To make matter easier, http://HomomorphicEncryption.org maintains a list of
    standardized bounds for the largest possible coefficient modulus per each
    choice of degree for the polynomial modulus. This list provides parameters
    guaranteeing 128-bit, 192-bit, and 256-bit security levels. This means that
    breaking the scheme should take at least 2^128, 2^192, or 2^256 operations,
    respectively, if instantiated with such parameters. We also note that 2^128
    operations is already far beyond what can be computed today, or in any
    conceivable near future. To protect the users against accidentally setting
    up insecure parameters, Microsoft SEAL by default enforces a security level
    of at least 128 bits.

    Microsoft SEAL also provides an easy way of selecting the coefficient modulus
    after the degree of the polynomial modulus is selected. These default moduli
    can be accessed through the functions

        DefaultParams::coeff_modulus_128(std::size_t poly_modulus_degree)
        DefaultParams::coeff_modulus_192(std::size_t poly_modulus_degree)
        DefaultParams::coeff_modulus_256(std::size_t poly_modulus_degree)

    for 128-bit, 192-bit, and 256-bit security levels, respectively. The integer
    parameter is the degree of the polynomial modulus, and no other value should
    ever be used in place of it, or else any security guarantees will be lost.

    In Microsoft SEAL the coefficient modulus is a positive composite number --
    a product of distinct primes of size up to 60 bits. When we talk about the
    size of the coefficient modulus we mean the bit length of the product of the
    primes. The small primes are represented by instances of the SmallModulus
    class, so for example DefaultParams::coeff_modulus_128(std::size_t) returns
    a vector of SmallModulus instances.

    In some cases expert users may want to customize their coefficient modulus.
    Since Microsoft SEAL uses the Number Theoretic Transform (NTT) for polynomial
    multiplications modulo the factors of the coefficient modulus, the factors
    need to be prime numbers congruent to 1 modulo 2*poly_modulus_degree. We have
    generated a list of such prime numbers of various sizes that the user can
    easily access through the functions

        DefaultParams::small_mods_60bit(std::size_t poly_modulus_degree)
        DefaultParams::small_mods_50bit(std::size_t poly_modulus_degree)
        DefaultParams::small_mods_40bit(std::size_t poly_modulus_degree)
        DefaultParams::small_mods_30bit(std::size_t poly_modulus_degree)

    each of which gives access to an array of primes of the denoted size. These
    primes are located in the source file seal/util/globals.cpp. For still more
    flexible prime selection, we have added a prime generation method

        SmallModulus::GetPrimes(
            int bit_size, std::size_t count, std::size_t ntt_size)

    that returns the largest `count' many primes with `bit_size' bits, supporting
    NTTs of size `ntt_size'. The parameter `ntt_size' should always be the degree
    of the polynomial modulus.

    Performance is mainly determined by the degree of the polynomial modulus, and
    the number of prime factors in the coefficient modulus; hence in some cases
    it can be important to use as few prime factors in the coefficient modulus
    as possible. However, there are scenarios demonstrated in these examples,
    where a user should instead choose more small primes than what the default
    parameters provide. The function

        DefaultParams::coeff_modulus_128(
            std::size_t poly_modulus_degree, std::size_t coeff_modulus_count)

    can be used to generate a desired number of small primes for 128-bit security
    level for a given degree of the polynomial modulus.

    In this example we use the default coefficient modulus for a 128-bit security
    level. Concretely, this coefficient modulus consists of two 36-bit and one
    37-bit prime factors: 0xffffee001, 0xffffc4001, 0x1ffffe0001.
    */
    parms.set_coeff_modulus(DefaultParams::coeff_modulus_128(4096));

    /*
    The plaintext modulus can be any positive integer, even though here we take
    it to be a power of two. In fact, in many cases one might instead want it
    to be a prime number; we will see this in later examples. The plaintext
    modulus determines the size of the plaintext data type and the consumption
    of noise budget in homomorphic (encrypted) multiplications. Thus, it is
    essential to try to keep the plaintext data type as small as possible for
    best performance. The noise budget in a freshly encrypted ciphertext is

        ~ log2(coeff_modulus/plain_modulus) (bits)

    and the noise budget consumption in a homomorphic multiplication is of the
    form log2(plain_modulus) + (other terms).
    The plaintext modulus is specific to the BFV scheme, and cannot be set when
    using the CKKS scheme.
    */
    parms.set_plain_modulus(256);

    /*
    Now that all parameters are set, we are ready to construct a SEALContext
    object. This is a heavy class that checks the validity and properties of the
    parameters we just set and performs several important pre-computations.
    */
    auto context = SEALContext::Create(parms);

    /*
    Print the parameters that we have chosen.
    */
    print_parameters(context);

    /*
    The encryption schemes in Microsoft SEAL are public key encryption schemes.
    For users unfamiliar with this terminology, a public key encryption scheme
    has a separate public key for encrypting data, and a separate secret key for
    decrypting data. This way multiple parties can encrypt data using the same
    shared public key, but only the proper recipient of the data can decrypt it
    with the secret key.

    We are now ready to generate the secret and public keys. For this purpose
    we need an instance of the KeyGenerator class. Constructing a KeyGenerator
    automatically generates the public and secret key, which can then be read to
    local variables.
    */
    KeyGenerator keygen(context);
    PublicKey public_key = keygen.public_key();
    SecretKey secret_key = keygen.secret_key();

    /*
    To be able to encrypt we need to construct an instance of Encryptor. Note
    that the Encryptor only requires the public key, as expected.
    */
    Encryptor encryptor(context, public_key);

    /*
    Computations on the ciphertexts are performed with the Evaluator class. In
    a real use-case the Evaluator would not be constructed by the same party
    that holds the secret key.
    */
    Evaluator evaluator(context);

    /*
    We will of course want to decrypt our results to verify that everything worked,
    so we need to also construct an instance of Decryptor. Note that the Decryptor
    requires the secret key.
    */
    Decryptor decryptor(context, secret_key);

    /*
    As an example, we evaluate the degree 4 polynomial

        2x^4 + 4x^3 + 4x^2 + 4x + 2

    over an encrypted x = 6. The coefficients of the polynomial can be considered
    as plaintext inputs, as we will see below. The computation is done modulo the
    plaintext modulus 256.

    While this examples is simple and easy to understand, it does not have much
    practical value. In later examples we will demonstrate how to compute more
    efficiently on encrypted integers and real numbers.
    */

    /*
    Plaintexts in the BFV scheme are polynomials of degree less than the degree
    of the polynomial modulus, and coefficients integers modulo the plaintext
    modulus. For reader with background in ring theory, the plaintext space is
    the polynomial quotient ring Z_T[X]/(X^N + 1), where N is poly_modulus_degree
    and T is plain_modulus.

    To get started, we create a plaintext containing the constant 6. For the
    plaintext element we use a constructor that takes the desired polynomial as
    a string with coefficients represented as hexadecimal numbers.
    */
    int x = 6;
    Plaintext plain_x(to_string(x));

    cout << "-- Express x = " << x << " as a plaintext polynomial 0x"
        << plain_x.to_string() << endl;

    /*
    We then encrypt the plaintext, producing a ciphertext.
    */
    Ciphertext encrypted_x;
    cout << "-- Encrypting plain_x: ";
    encryptor.encrypt(plain_x, encrypted_x);
    cout << "Done (encrypted_x)" << endl;

    /*
    In Microsoft SEAL, a valid ciphertext consists of two or more polynomials
    whose coefficients are integers modulo the product of the primes in the
    coefficient modulus. The number of polynomials in a ciphertext is called its
    `size' and is given by Ciphertext::size(). A freshly encrypted ciphertext
    always has size 2.
    */
    cout << "\tSize of freshly encrypted x: " << encrypted_x.size() << endl;

    /*
    There is plenty of noise budget left in this freshly encrypted ciphertext.
    */
    cout << "\tNoise budget in freshly encrypted x: "
        << decryptor.invariant_noise_budget(encrypted_x) << " bits" << endl;

    /*
    We decrypt the ciphertext and print the resulting plaintext in order to
    demonstrate correctness of the encryption.
    */
    Plaintext decrypted_x;
    cout << "   Decrypting encrypted_x: ";
    decryptor.decrypt(encrypted_x, decrypted_x);
    cout << "Done (decrypted_x = 0x" << decrypted_x.to_string() << ")" << endl;

    /*
    When using Microsoft SEAL, it is typically advantageous to compute in a way
    that minimizes the longest chain of sequential homomorphic multiplications.
    In other words, homomorphic computations are best evaluated in a way that
    minimizes the multiplicative depth. This is because the total noise budget
    consumption is proportional to the multiplicative depth. Therefore, in this
    example it is advantageous to factorize the polynomial as

        2x^4 + 4x^3 + 4x^2 + 4x + 2 = 2(x + 1)^2 * (x^2 + 1),

    to obtain a simple depth 2 representation. Thus, we compute (x + 1)^2 and
    (x^2 + 1) separately, before multiplying them, and multiplying by 2.
    */

    /*
    First, we compute x^2 and add a plaintext "1". We can clearly see from the
    print-out that multiplication has consumed a lot of noise budget. The user
    can change the plain_modulus parameter to see its effect on the rate of noise
    budget consumption.
    */
    cout << "-- Computing x^2+1: ";
    Ciphertext x_square_plus_one;
    evaluator.square(encrypted_x, x_square_plus_one);
    Plaintext plain_one("1");
    evaluator.add_plain_inplace(x_square_plus_one, plain_one);
    cout << "Done" << endl;

    /*
    Homomorphic multiplication results in the output ciphertext growing in size.
    More precisely, if the input ciphertexts have size M and N, then the output
    ciphertext after homomorphic multiplication will have size M+N-1. In this
    case we perform squaring to observe this growth (also observe noise budget
    consumption).
    */
    cout << "\tSize of x^2+1: " << x_square_plus_one.size() << endl;
    cout << "\tNoise budget in x^2+1: "
        << decryptor.invariant_noise_budget(x_square_plus_one) << " bits" << endl;

    /*
    It does not matter that the size has grown -- decryption works as usual, as
    long as noise budget has not reached 0.
    */
    Plaintext decrypted_result;
    cout << "   Decrypting x^2+1: ";
    decryptor.decrypt(x_square_plus_one, decrypted_result);
    cout << "Done (x^2+1 = 0x" << decrypted_result.to_string() << ")" << endl;

    /*
    Next, we compute (x + 1)^2.
    */
    cout << "-- Computing (x+1)^2: ";
    Ciphertext x_plus_one_square;
    evaluator.add_plain(encrypted_x, plain_one, x_plus_one_square);
    evaluator.square_inplace(x_plus_one_square);
    cout << "Done" << endl;
    cout << "\tSize of (x+1)^2: " << x_plus_one_square.size() << endl;
    cout << "\tNoise budget in (x+1)^2: "
        << decryptor.invariant_noise_budget(x_plus_one_square) << " bits" << endl;
    cout << "   Decrypting (x+1)^2: ";
    decryptor.decrypt(x_plus_one_square, decrypted_result);
    cout << "Done ((x+1)^2 = 0x" << decrypted_result.to_string() << ")" << endl;

    /*
    Finally, we multiply (x^2 + 1), (x + 1)^2, and 2.
    */
    cout << "-- Computing 2(x^2+1)(x+1)^2: ";
    Ciphertext encrypted_result;
    evaluator.multiply(x_square_plus_one, x_plus_one_square, encrypted_result);
    Plaintext plain_two("2");
    evaluator.multiply_plain_inplace(encrypted_result, plain_two);
    cout << "Done" << endl;
    cout << "\tSize of 2(x^2+1)(x+1)^2: " << encrypted_result.size() << endl;
    cout << "\tNoise budget in 2(x^2+1)(x+1)^2: "
        << decryptor.invariant_noise_budget(encrypted_result) << " bits" << endl;
    cout << "NOTE: Decryption can be incorrect if noise budget is zero." << endl;
    cout << endl;

    /*
    Noise budget has reached 0, which means that decryption cannot be expected to
    give the correct result. This is because both ciphertexts x_square_plus_one
    and x_plus_one_square consist of 3 polynomials due to the previous squaring
    operations, and homomorphic operations on large ciphertexts consume much more
    noise than computations on small ciphertexts. Computing on smaller ciphertexts
    is also computationally significantly cheaper.

    `Relinearization' is an operation that reduces the size of a ciphertext after
    multiplication back to the initial size, 2. Thus, relinearizing one or both
    input ciphertexts before the next multiplication can have a huge positive
    impact on both noise growth and performance, even though relinearization has
    a significant computational cost itself.

    Relinearization requires a special type of relinearization key, which can be
    thought of as a kind of public key. Relinerization keys can easily be created
    with the KeyGenerator. To relinearize a ciphertext of size M >= 2 back to
    size 2, we actually need M-2 relinearization keys. Attempting to relinearize
    a too large ciphertext with too few relinearization keys will result in an
    exception being thrown.

    Relinearization is used similarly in both the BFV and the CKKS schemes, but
    in this example we continue using BFV. We repeat our computation from before,
    but this time relinearize after every multiplication.

    Microsoft SEAL has implements a very efficient relinearization algorithm,
    making it computationally efficient and almost free in terms of noise budget
    consumption. For these reasons, relinearizing after every multiplication is
    a generally recommended approach. Note that in doing so, our ciphertexts will
    only ever reach size 3; hence it suffices to generate a single relinearization
    key.
    */

    /*
    We use KeyGenerator::relin_keys to create a single relinearization key.
    Another overload of the function would take the number of relinearization keys
    as an argument, but since we decided to relinearize after every multiplication,
    one key is all we need.
    */
    cout << "-- Generating relinearization keys: ";
    auto relin_keys = keygen.relin_keys();
    cout << "Done" << endl;

    /*
    Now repeat the computation and relinearize after each multiplication.
    */
    cout << "-- Computing x^2: ";
    evaluator.square(encrypted_x, x_square_plus_one);
    cout << "Done" << endl;
    cout << "\tSize of x^2: " << x_square_plus_one.size() << endl;
    cout << "-- Relinearizing x^2: ";
    evaluator.relinearize_inplace(x_square_plus_one, relin_keys);
    cout << "Done" << endl;
    cout << "\tSize of x^2 (after relinearization): "
        << x_square_plus_one.size() << endl;
    cout << "-- Computing x^2+1: ";
    evaluator.add_plain_inplace(x_square_plus_one, plain_one);
    cout << "Done" << endl;
    cout << "\tNoise budget in x^2+1: "
        << decryptor.invariant_noise_budget(x_square_plus_one) << " bits" << endl;

    cout << "-- Computing x+1: ";
    evaluator.add_plain(encrypted_x, plain_one, x_plus_one_square);
    cout << "Done" << endl;
    cout << "-- Computing (x+1)^2: ";
    evaluator.square_inplace(x_plus_one_square);
    cout << "Done" << endl;
    cout << "\tSize of (x+1)^2: " << x_plus_one_square.size() << endl;
    cout << "-- Relinearizing (x+1)^2: ";
    evaluator.relinearize_inplace(x_plus_one_square, relin_keys);
    cout << "Done" << endl;
    cout << "\tSize of (x+1)^2 (after relinearization): "
        << x_plus_one_square.size() << endl;
    cout << "\tNoise budget in (x+1)^2: "
        << decryptor.invariant_noise_budget(x_plus_one_square) << " bits" << endl;

    cout << "-- Computing (x^2+1)(x+1)^2: ";
    evaluator.multiply(x_square_plus_one, x_plus_one_square, encrypted_result);
    cout << "Done" << endl;
    cout << "\tSize of (x^2+1)(x+1)^2: " << encrypted_result.size() << endl;
    cout << "-- Relinearizing (x^2+1)(x+1)^2: ";
    evaluator.relinearize_inplace(encrypted_result, relin_keys);
    cout << "Done" << endl;
    cout << "\tSize of (x^2+1)(x+1)^2 (after relinearization): "
        << encrypted_result.size() << endl;
    cout << "-- Computing 2(x^2+1)(x+1)^2: ";
    evaluator.multiply_plain_inplace(encrypted_result, plain_two);
    cout << "Done" << endl;
    cout << "\tNoise budget in 2(x^2+1)(x+1)^2: "
        << decryptor.invariant_noise_budget(encrypted_result) << " bits" << endl;
    cout << "NOTE: Notice the increase in remaining noise budget." << endl;

    /*
    Relinearization clearly improved our noise consumption. Since we still have
    noise budget left, decryption should work correctly.
    */
    cout << "-- Decrypting 2(x^2+1)(x+1)^2: ";
    decryptor.decrypt(encrypted_result, decrypted_result);
    cout << "Done (2(x^2+1)(x+1)^2 = 0x" << decrypted_result.to_string() << ")" << endl;
    cout << endl;

    /*
    For x=6, 2(x^2+1)(x+1)^2 = 3626. Since the plaintext modulus is set to 256,
    this result is computed in integers modulo 256. Therefore the expected output
    should be 3626 % 256 == 42, or 0x2A in hexadecimal.
    */
}