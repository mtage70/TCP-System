CS3600 Project 4 README
TCP
Matthew Miller, John Kelly, Garin Bedian

Our approach for this project began by deciding how to simulate a windowed protocol.
At first we depended on a system of integers to indicate to the TCP which sequence number
is at the beginning of the window and which one is at the end, allowing us to step through
a multitude of packets. However, this approach became highly inefficient when we considered retransmissions due
to dropped or corrupted packets. To solve this issue we transfered to an Array system (a buffer) which could hold
the information that had been sent but not acknowledged by the receiver. This allowed us to isolate which
packet needed to be retransmitted from the window at any given point, allowing for a reliable transfer protocol.
To increase our sending speed, we developed a method to perform fast retransmission for our TCP. This addition
helped our TCP operate more efficiently because our sender would not wait as long to retransmit when a packet
had been dropped. 

The biggest challenge was the resculpting of the entire system when we decided to try a different tactic with the
Array. The previous integer system was highly inefficient for our purposes, but it was very simple to understand.
When we exchanged this system for the Array, it was noticably more efficient but we experienced much more conceptual
error than we had with the previous approach.

We are especially proud of our use of checksums as a way of verifying data integrity between sender and receiver. It
was a more efficient security feature for our TCP overall, but writting the code necessary to implement it was
a big challenge and took a lot of time to successfully pull off. Also implementing Fast Retransmission was an 
endeavor all on it's own. When we examined our TCP's speed we felt it was necessary to include such a feature,
but we had to do a lot of tweaking to the code in order to fully optimize it.

Overall, our process determined the features and properties of our TCP. As we tested on different emulated networks,
we saw what we were doing well and what still needed work. For the most part we found our system was usually
reliable but the main drawback was it's speed. The features we then implemented were a direct result of the constant
feedback we received from the ./run checks.