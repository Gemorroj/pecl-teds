<?php

/** @generate-class-entries */
// Stub generation requires build/gen_stub.php from php 8.1 or newer.

namespace Teds;

/**
 * Set using a stable sort, which can contain any value type.
 * Iteration will return values in the order they would be sorted by `Teds\stable_compare`, ascending.
 * This is a set where entries for keys of any type can be created if `Teds\stable_compare($key1, $key2) != 0`,
 * otherwise the previous entry is deleted.
 *
 * **This is a work in progress.** Iteration will not work as expected if values are removed during iteration,
 * and this is backed by a raw array (using insertion sort) instead of a balanced binary tree,
 * meaning that insertion/removals are inefficient (linear time to move values) for large sets.
 */
final class SortedStrictSet implements \IteratorAggregate, \Countable, \JsonSerializable
{
    /** Construct the SortedStrictSet from the keys and values of the Traversable/array. */
    public function __construct(iterable $iterator = []) {}
    /**
     * Returns an iterator over the values of the SortedStrictSet.
     * key() and value() both return the same value.
     * Elements are iterated over in sorted order.
     */
    public function getIterator(): \InternalIterator {}
    /** Returns the number of values in the SortedStrictSet. */
    public function count(): int {}
    /** Returns true if there are 0 values in the SortedStrictSet. */
    public function isEmpty(): bool {}
    /** Removes all elements from the SortedStrictSet. */
    public function clear(): void {}

    /**
     * Returns a list of the values in order of insertion.
     * @implementation-alias Teds\SortedStrictSet::values
     */
    public function __serialize(): array {}
    public function __unserialize(array $data): void {}
    /** Construct the SortedStrictSet from values of the array. */
    public static function __set_state(array $array): SortedStrictSet {}

    /** Returns a list of the unique values in order of insertion. */
    public function values(): array {}

    /** Returns the first value, throws if empty. */
    public function bottom(): mixed {}

    /** Returns the last value, throws if empty */
    public function top(): mixed {}

    /**
     * Pops a value from the end of the SortedStrictSet.
     * @throws \UnderflowException if the SortedStrictSet is empty
     */
    public function pop(): mixed {}
    /**
     * Shifts a value from the front of the SortedStrictSet
     * @throws \UnderflowException if the SortedStrictSet is empty
     */
    public function shift(): mixed {}
    /**
     * Returns true if $value exists in this SortedStrictSet, false otherwise.
     */
    public function contains(mixed $value): bool {}

    /**
     * Returns true if $value was added to this SortedStrictSet and was not previously in this SortedStrictSet.
     */
    public function add(mixed $value): bool {}

    /**
     * Returns true if $value was found in this SortedStrictSet before being removed from this SortedStrictSet.
     */
    public function remove(mixed $value): bool {}

    /**
     * Returns [v1, v2, ...]
     * @implementation-alias Teds\SortedStrictSet::values
     */
    public function jsonSerialize(): array {}
}
