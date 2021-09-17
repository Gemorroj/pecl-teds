<?php

/** @generate-class-entries */

namespace Teds;

final class Vector implements \IteratorAggregate, \Countable, \JsonSerializable, \ArrayAccess
{
    /**
     * Construct a Vector from an iterable.
     *
     * When $preserveKeys is false, the values will be reindexed without gaps starting from 0
     * When $preserveKeys is true, any gaps in the keys of the iterable will be filled in with null,
     * and negative indices or non-integer indices will be rejected and cause an Exception.
     */
    public function __construct(iterable $iterator = [], bool $preserveKeys = true) {}
    public function getIterator(): \InternalIterator {}
    /**
     * Returns the number of values in this Vector
     */
    public function count(): int {}
    /**
     * Returns the total capacity of this Vector.
     */
    public function capacity(): int {}
    /**
     * Reduces the Vector's capacity to its size, freeing any extra unused memory.
     */
    public function shrinkToFit(): void {}
    /**
     * If the current capacity is less than $capacity, raise it to capacity.
     * @throws UnexpectedValueException if the new capacity is too large
     */
    public function reserve(int $capacity): void {}
    /**
     * Remove all elements from the array and free all reserved capacity.
     */
    public function clear(): void {}

    /**
     * If $size is greater than the current size, raise the size and fill the free space with $value
     * If $size is less than the current size, reduce the size and discard elements.
     */
    public function setSize(int $size, mixed $value = null): void {}

    public function __serialize(): array {}
    public function __unserialize(array $data): void {}
    public static function __set_state(array $array): Vector {}

    public function push(mixed $value): void {}
    public function pop(): mixed {}

    public function toArray(): array {}
    // Strictly typed, unlike offsetGet/offsetSet
    public function get(int $offset): mixed {}
    public function set(int $offset, mixed $value): void {}

    /**
     * Returns the value at (int)$offset.
     * @throws \OutOfBoundsException if the value of (int)$offset is not within the bounds of this vector
     */
    public function offsetGet(mixed $offset): mixed {}
    /**
     * Returns true if `0 <= (int)$offset && (int)$offset < $this->count().
     */
    public function offsetExists(mixed $offset): bool {}

    /**
     * @throws \OutOfBoundsException if the value of (int)$offset is not within the bounds of this vector
     */
    public function offsetSet(mixed $offset, mixed $value): void {}

    /**
     * @throws \RuntimeException because unset and null are different things, unlike SplFixedArray
     */
    public function offsetUnset(mixed $offset): void {}

    /**
     * Returns the offset of a value that is === $value, or returns null.
     */
    public function indexOf(mixed $value): ?int {}
    /**
     * @return bool true if there exists a value === $value in this vector.
     */
    public function contains(mixed $value): bool {}

    /**
     * Returns a new Vector instance created from the return values of $callable($element)
     * being applied to each element of this vector.
     *
     * (at)param null|callable(mixed):mixed $callback
     */
    public function map(callable $callback): Vector {}
    /**
     * Returns the subset of elements of the Vector satisfying the predicate.
     *
     * If the value returned by the callback is truthy
     * (e.g. true, non-zero number, non-empty array, truthy object, etc.),
     * this is treated as satisfying the predicate.
     *
     * (at)param null|callable(mixed):bool $callback
     */
    public function filter(?callable $callback = null): Vector {}

    public function jsonSerialize(): array {}
}
