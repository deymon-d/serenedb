#!/usr/bin/python3

import random
import time
import string

from typing import TextIO

class Config:
    DocCount = 20000
    DataSize = 200
    BigDataDocCount = 10
    BigDataSize = 20000
    ResourceRootPath = "resources/tests/iresearch"
    BenchResourceFilePrefix = "interval_bench_"
    BigDataBenchResourceFilePrefix = f"{BenchResourceFilePrefix}big_data_"
    RepeatPhraseBenchResource = f"{ResourceRootPath}/{BenchResourceFilePrefix}repeat.json"
    FreqsEqualBenchResource = f"{ResourceRootPath}/{BenchResourceFilePrefix}freqs_equal.json"
    FreqsDiscreteBenchResource = f"{ResourceRootPath}/{BenchResourceFilePrefix}freqs_discrete.json"
    BigDataRepeatPhraseBenchResource = f"{ResourceRootPath}/{BigDataBenchResourceFilePrefix}repeat.json"
    BigDataFreqsEqualBenchResource = f"{ResourceRootPath}/{BigDataBenchResourceFilePrefix}freqs_equal.json"
    BigDataFreqsDiscreteBenchResource = f"{ResourceRootPath}/{BigDataBenchResourceFilePrefix}freqs_discrete.json"


class DataGenerator:
    def __init__(self, file: TextIO, name: str, data_size: int):
        self.file = file
        self.name = name
        self.data = bytearray(data_size)

    def get_next_token(self) -> bytearray:
        raise NotImplementedError()

    def generate(self) -> None:
        pos = 0
        while pos != len(self.data):
            token = self.get_next_token()
            if pos + len(token) > len(self.data):
                self.data[pos:] = token[0:(len(self.data) - pos)]
                break
            self.data[pos:(pos + len(token))] = token
            pos += len(token)
            if pos < len(self.data):
                self.data[pos:(pos + 1)] = bytearray(' ', encoding="utf8")
                pos += 1

        print(f'\t{{"name":"{self.name}","phrase":"{self.data.decode("utf8")}"}}', file=self.file, end='')

    def refresh(self, name: str) -> None:
        self.data: bytearray = bytearray(len(self.data))
        self.name = name


class RepeatDataGenerator(DataGenerator):
    def __init__(self, file: str, name: str, data_size: int, repeat_pattern: list[str]):
        super().__init__(file, name, data_size)
        self.pattern = list(map(lambda x: bytearray(x, encoding="utf8"), repeat_pattern))
        self.pos = 0

    def get_next_token(self):
        token = self.pattern[self.pos]
        self.pos = (self.pos + 1) % len(self.pattern)
        return token


class FreqsDataGenerator(DataGenerator):
    def __init__(self, file: TextIO, name: str, data_size: int, tokens_with_freqs: dict[str, float]):
        super().__init__(file, name, data_size)
        self.pattern : list[bytearray] = []
        self.freqs : list[float] = []
        for token, freq in tokens_with_freqs.items():
            self.pattern.append(bytearray(token, encoding="utf8"))
            self.freqs.append(freq)
        total = sum(self.freqs)
        if abs(total - 1.0) > 0.0001:
            raise ValueError(f"Сумма частот должна быть равна 1.0 (текущая: {total})")
        self.rng = random.Random()
        self.rng.seed(int(time.time() * 1_000_000))

    def get_next_token(self):
        return self.pattern[self.rng.choices(
            population=range(len(self.freqs)),
            weights=self.freqs,
            k=1
        )[0]]


def generate_random_string(length=10, seed=None):
    if seed is not None:
        random.seed(seed)
    
    characters = string.ascii_letters + string.digits
    return ''.join(random.choice(characters) for _ in range(length))


def generation_loop(generator: type[DataGenerator], file: str, doc_count: int, *args, **kwargs):
    name = 1
    names = set()

    def gen_next_name(seed: int):
        cand = generate_random_string(10, seed)
        while cand in names:
            seed += 1
            cand = generate_random_string(10, seed)
        names.add(cand)
        return cand
    
    with open(file, "w") as f:
        print("[", file=f)
        gen = generator(f, gen_next_name(name), *args, **kwargs)
        for iter in range(doc_count - 1):
            gen.generate()
            print(",", file=f)
            name += 1
            gen.refresh(gen_next_name(name))
        gen.generate()
        print("\n]", file=f)


def main():
    data = ["fox", "quick", "brown", "jumps", "second", "dog"]
    freq_eq = [0.1, 0.1, 0.1, 0.1, 0.3, 0.3]
    custom_freqs = [0.2, 0.1, 0.05, 0.1, 0.25, 0.3]

    def combine(tokens: list[str], freqs: list[float]):
        return dict(zip(tokens, freqs))

    generation_loop(RepeatDataGenerator, Config.RepeatPhraseBenchResource, Config.DocCount, Config.DataSize, data)
    generation_loop(FreqsDataGenerator, Config.FreqsEqualBenchResource, Config.DocCount, Config.DataSize, combine(data, freq_eq))
    generation_loop(FreqsDataGenerator, Config.FreqsDiscreteBenchResource, Config.DocCount, Config.DataSize, combine(data, custom_freqs))
    generation_loop(RepeatDataGenerator, Config.BigDataRepeatPhraseBenchResource, Config.BigDataDocCount, Config.BigDataSize, data)
    generation_loop(FreqsDataGenerator, Config.BigDataFreqsEqualBenchResource, Config.BigDataDocCount, Config.BigDataSize, combine(data, freq_eq))
    generation_loop(FreqsDataGenerator, Config.BigDataFreqsDiscreteBenchResource, Config.BigDataDocCount, Config.BigDataSize, combine(data, custom_freqs))

if __name__ == "__main__":
    main()
