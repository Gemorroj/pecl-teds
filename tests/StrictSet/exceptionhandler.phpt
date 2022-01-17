--TEST--
Teds\StrictSet constructed from Traversable throwing
--FILE--
<?php

class HasDestructor {
    public function __construct(public string $value) {}
    public function __destruct() {
        echo "in " . __METHOD__ . " $this->value\n";
    }
}

function yields_and_throws() {
    yield 123 => new HasDestructor('in value');
    yield new HasDestructor('in key') => 123;
    echo "Key should have been freed\n";
    yield 123 => new HasDestructor('in value');
    yield 'first' => 'second';

    throw new RuntimeException('test');

    echo "Unreachable\n";
}
try {
    $it = new Teds\StrictSet(yields_and_throws());
} catch (RuntimeException $e) {
    echo "Caught " . $e->getMessage() . "\n";
}
gc_collect_cycles();
echo "Done\n";
?>
--EXPECT--
Key should have been freed
in HasDestructor::__destruct in key
in HasDestructor::__destruct in value
in HasDestructor::__destruct in value
Caught test
Done