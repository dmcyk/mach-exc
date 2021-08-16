//
//  main.m
//  MachExc
//
//  Created by Damian Malarczyk on 16/08/2021.
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"

int __main(void);

int main(int argc, char * argv[]) {
  NSString * appDelegateClassName;
  @autoreleasepool {
    __main();
      // Setup code that might create autoreleased objects goes here.
      appDelegateClassName = NSStringFromClass([AppDelegate class]);
  }
  return UIApplicationMain(argc, argv, nil, appDelegateClassName);
}
