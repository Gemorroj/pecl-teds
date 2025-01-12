--TEST--
Teds\Deque offsetSet/set
--FILE--
<?php

function expect_throws(Closure $cb): void {
    try {
        $cb();
        echo "Unexpectedly didn't throw\n";
    } catch (Throwable $e) {
        printf("Caught %s: %s\n", $e::class, $e->getMessage());
    }
}

echo "Test empty deque\n";
$it = new Teds\Deque([]);
expect_throws(fn() => $it->offsetSet(0, strtoupper('value')));
expect_throws(fn() => $it->set(0, strtoupper('value')));

echo "Test short deque\n";
$str = 'Test short deque';
$it = new Teds\Deque(explode(' ', $str));
$it->set(0, 'new');
$it->offsetSet(2, strtoupper('test'));
echo json_encode($it), "\n";
expect_throws(fn() => $it->set(-1, strtoupper('value')));
expect_throws(fn() => $it->set(3, 'end'));
expect_throws(fn() => $it->set(PHP_INT_MAX, 'end'));

?>
--EXPECT--
Test empty deque
Caught OutOfBoundsException: Index out of range
Caught OutOfBoundsException: Index out of range
Test short deque
["new","short","TEST"]
Caught OutOfBoundsException: Index out of range
Caught OutOfBoundsException: Index out of range
Caught OutOfBoundsException: Index out of range