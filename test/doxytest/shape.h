/** \file 
 * <pre><b>Richard Zeng Shape Class File Source</b></pre>
 *  <pre><b>Copyright and Use</b></pre>
 *
 * \author Richard Zeng
 * \date 2006-3-23
 *
 * <pre>zengyongjoy@gmail.com</pre>
 * <b>All rights reserved.</b>
 */

/** namespace test define
 */
namespace test {
/** class shape define
 * this is the base class for all Shape
 */
class Shape{
 public :
  Shape();
  ~Shape();
  /** interface */
  virtual void Draw(CDC* pDC) = 0;
};

/** Rectangle class define
 */
class Rectangle : public Shape {
 public:
  Rectangle();
  ~Rectangle();
  virtual void Draw(CDC* pDC);
 private:
  int width, height;
};

};
